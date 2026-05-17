#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include <thread>
#include <print>
#include <fstream>
#include <argparse/argparse.hpp>
#include <chrono>
#include "noise.hpp"

template<typename SeedCache>
struct Seed {
    uint32_t seed;
    ElevationType elevation_type;
    size_t water_scale_idx;
    size_t water_coverage_idx;
    float score;
    SeedCache cache;
};

template<>
struct Seed<void> {
    uint32_t seed;
    ElevationType elevation_type;
    size_t water_scale_idx;
    size_t water_coverage_idx;
    float score;
};
    
struct SeedComparator {
    template<typename T>
    bool operator()(const Seed<T> &a, const Seed<T> &b) const {
        return a.score > b.score;
    }
};

/**
 * A simple pipeline to analyze seeds.
 * 
 * It works in stages: the first stage is ran for every seed,
 * and the other stages are only ran for the seeds that passed the previous stage.
 * It is made that way because certain criteria may be easier to compute than other,
 * and running the expensive criteria on every seed may take too long.
 * For instance, generating the positions/radii of regular patches is around 10x faster than
 * generating the same for starter patches + there are 72x more configurations of starter patches
 * than regular patches because of water settings and twin seeds. So on my computer
 * generating the regular patches take ~10min but the same with starter patches would take multiple days.
 * Moreover, a full scan with water or the exact shape of patches would probably
 * take more like a month with my computer.
 * We can use this by eliminating as many seeds as possible during the first stage using only
 * regular patches, and then refining the filtering with more precise criteria in later stages.
 */
template<typename SeedCache = void>
class Finder {
public:
    struct EvalResult {
        // If true, score is ignored and the seed will instantly be eliminated.
        // It should be true for as many seeds as possible, in particular during the first stage
        // as inserting seeds into the seed list is not very fast.
        bool eliminate;
        float score;
    };

    using EvalFunction = std::function<
        EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*)
    >;
    template<typename T>
    using EvalFunctionWithCache = std::function<
        EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*, T& stage_cache)
    >;

    struct StageSettings {
        // For a given integer n, the regular patches are the exact same for the seeds 2n and 2n+1.
        // So only looking at even seeds is enough check them.
        //
        // True => check all seeds.
        // False => only check even number seeds.
        // If any stage has this set to true, it will true for 
        // every following stages, ignoring the setting.
        bool check_twin_seeds = false;

        // If any stage has this set to true, it will true for 
        // every following stages, ignoring the setting.
        bool check_water_settings = false;

        // If any stage has this set to true, it will true for 
        // every following stages, ignoring the setting.
        bool check_elevation_types = false;

        // Defines how many seeds will be passed to the next stage.
        // For the last stage, defines how many seeds will be put in the output file.
        uint32_t seed_nb_to_next_stage = SEED_MAX;
    };

    Finder(const MapGenSettings& settings) : _map_gen_settings(settings) {
        _water_scales = { settings.water_scale };
        _water_coverages = { settings.water_coverage };
    }

    // Defaults to the water scale given by the constructor MapGenSettings argument
    void set_water_scales(std::vector<float> scales) {
        _water_scales = scales;
    }

    // Defaults to the water coverage given by the constructor MapGenSettings argument
    void set_water_coverages(std::vector<float> coverages) {
        _water_coverages = coverages;
    }

    void set_elevation_types(std::vector<ElevationType> types) {
        _elevation_types = types;
    }

    /**
     * EvalFunction should return the new score.
     * SeedScorePair contains the seed and its score from the previous stage.
     * NoisePrecompute and NoiseCache may be used by noise functions.
     * 
     * Stages are ran in order of insertion.
     */
    inline void add_stage(EvalFunction eval_function, StageSettings settings) {
        _stages.emplace_back(
            [=](const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& n_cache,
                uint32_t seed, SeedCache* seed_cache, void*) -> EvalResult {
                return eval_function(settings, precompute, n_cache, seed, seed_cache);
            },
            settings,
            [] { return nullptr; },
            [](void*) { return; }
        );
    }
    /**
     * Same as add_stage but the eval function gets a reference to a stage cache.
     * The cache can be of any type and there is one per worker, so it does not need
     * to be thread safe. This cache main use case is if you need some very big array
     * for your eval function, that array should be in the cache, so you don't need
     * to reallocate it every time.
     */
    template<typename StageCacheType>
    void add_stage_with_cache(EvalFunctionWithCache<StageCacheType> eval_function, StageSettings settings) {
        _stages.emplace_back(
            [=](const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& n_cache,
                uint32_t seed, SeedCache* seed_cache, void* cache) -> EvalResult {
                return eval_function(settings, precompute, n_cache, seed, seed_cache, *(StageCacheType*)cache);
            },
            settings,
            [] { return new StageCacheType; },
            [](void* cache) { delete (StageCacheType*)cache; }
        );
    }

    int run(std::string program_name, int argc, char* argv[]);

private:
    struct Stage {
        std::function<
            EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, uint32_t seed, SeedCache*, void* stage_cache)
        > eval_function;
        StageSettings settings;
        std::function<void*()> cache_factory;
        std::function<void(void*)> cache_dealloc;
    };

    void worker_first_stage(int id, const Stage&, const std::vector<NoisePrecompute>&,
        bool check_twin_seeds, bool check_water_settings, bool check_elevation_types, std::atomic<uint64_t>& progress);
    void worker_other_stages(int id, const Stage&, const std::vector<NoisePrecompute>&,
        bool check_twin_seeds, bool check_water_settings, bool check_elevation_types, std::atomic<uint64_t>& progress);

    struct RunOptions {
        int threads;
        uint32_t first_stage_first_seed;
        uint32_t first_stage_last_seed;
    };

    RunOptions _run_options;
    MapGenSettings _map_gen_settings;
    std::vector<float> _water_scales;
    std::vector<float> _water_coverages;
    std::vector<ElevationType> _elevation_types = { ELEVATION_2_0, ELEVATION_1_1, ELEVATION_ISLAND };

    bool _elevation_types_already_checked = false;
    bool _twin_seeds_already_checked = false;
    bool _water_settings_already_checked = false;
    std::vector<Stage> _stages;
    TopK<Seed<SeedCache>, SeedComparator> _previous_top_seeds;
    TopK<Seed<SeedCache>, SeedComparator> _top_seeds;
};

template<typename SeedCache>
int Finder<SeedCache>::run(std::string program_name, int argc, char* argv[]) {
    if (_stages.size() == 0) {
        std::cerr << "At least one stage must be added before calling Finder::run." << std::endl;
        return 1;
    }

    argparse::ArgumentParser program(program_name);

    program.add_argument("-o", "--output")
        .help("The path to the output file.")
        .required()
        .metavar("PATH");

    program.add_argument("--threads")
        .help("Number of threads to use.")
        .default_value(1)
        .scan<'i', int>()
        .metavar("INT");

    program.add_argument("--first-seed")
        .help("The first seed to be scanned by the first stage. Must be an even number.")
        .default_value(0u)
        .scan<'i', uint32_t>()
        .metavar("UINT32");

    program.add_argument("--last-seed")
        .help("The last seed to be scanned by the first stage.")
        .default_value(SEED_MAX)
        .scan<'i', uint32_t>()
        .metavar("UINT32");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    auto out_path = program.get<std::string>("-o");

    _run_options.threads = program.get<int>("--threads");
    std::println("Threads count set to {}.", _run_options.threads);

    _run_options.first_stage_first_seed = program.get<uint32_t>("--first-seed");
    std::println("First seed is {}.", _run_options.first_stage_first_seed);

    _run_options.first_stage_last_seed = program.get<uint32_t>("--last-seed");
    std::println("Last seed is {}.", _run_options.first_stage_last_seed);

    if (_run_options.first_stage_first_seed > _run_options.first_stage_last_seed) {
        std::println("The first seed must be less than the last seed.");
        return 1;
    }

    std::print("Precomputing noise data...");
    std::vector<NoisePrecompute> precomputes;
    for (size_t scale = 0; scale < _water_scales.size(); scale++) {
        _map_gen_settings.water_scale = _water_scales[scale];
        for (size_t coverage = 0; coverage < _water_coverages.size(); coverage++) {
            _map_gen_settings.water_coverage = _water_coverages[coverage];
            precomputes.emplace_back(_map_gen_settings);
        }
    }
    std::println(" Done.");

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t stage_idx = 0; stage_idx < _stages.size(); stage_idx++) {
        const auto& stage = _stages[stage_idx];
        const auto& stage_settings = stage.settings;

        bool check_twin_seeds = false;
        if (_twin_seeds_already_checked == false && stage_settings.check_twin_seeds == true) {
            check_twin_seeds = true;
            _twin_seeds_already_checked = true;
        }
        if (check_twin_seeds) {
            std::println("Adding twin seeds (increases the number of seeds by 2x)");
        }

        bool check_water_settings = false;
        if (_water_settings_already_checked == false && stage_settings.check_water_settings == true) {
            check_water_settings = true;
            _water_settings_already_checked = true;
        }
        if (check_water_settings) {
            std::println("Adding different water settings (increases the number of seeds by {}x)", _water_scales.size() * _water_coverages.size());
        }

        bool check_elevation_types = false;
        if (_elevation_types_already_checked == false && stage_settings.check_elevation_types == true) {
            check_elevation_types = true;
            _elevation_types_already_checked = true;
        }
        if (check_elevation_types) {
            std::println("Adding different elevation types (increases the number of seeds by {}x)", _elevation_types.size());
        }

        std::atomic<uint64_t> progress;
        uint64_t total;
        if (stage_idx == 0) {
            total = ((_run_options.first_stage_last_seed - _run_options.first_stage_first_seed) / 2) * 2;
        } else {
            _previous_top_seeds = std::move(_top_seeds);
            total = (uint64_t)_previous_top_seeds.size();
            if (check_twin_seeds) total *= 2;
            if (check_water_settings) total *= (uint64_t)(_water_scales.size() * _water_coverages.size());
            if (check_elevation_types) total *= (uint64_t)(_elevation_types.size());
        }

        _top_seeds.clear();
        _top_seeds.set_max_size(stage_settings.seed_nb_to_next_stage);

        std::vector<std::jthread> threads;
        for (int id = 0; id < _run_options.threads; id++) {
            std::jthread thread;
            if (stage_idx == 0) {
                thread = std::jthread([&, id, check_twin_seeds] {
                    worker_first_stage(id, stage, precomputes, check_twin_seeds, check_water_settings, check_elevation_types, progress);
                });
            } else {
                thread = std::jthread([&, id, check_twin_seeds] {
                    worker_other_stages(id, stage, precomputes, check_twin_seeds, check_water_settings, check_elevation_types, progress);
                });
            }
            threads.push_back(std::move(thread));
        }

        uint64_t previous_progress = 0;
        auto stage_start = std::chrono::high_resolution_clock::now();
        auto previous_stage_time = stage_start;
        while (progress.load() < total) {
            uint64_t current_progress = progress.load();
            uint64_t diff_progress = current_progress - previous_progress;
            auto current_time = std::chrono::high_resolution_clock::now();
            auto diff_time = current_time - previous_stage_time;

            using namespace std::chrono_literals;
            auto eta = std::chrono::duration_cast<std::chrono::seconds>(
                diff_progress == 0 ? 0s : (total - current_progress) * (diff_time / diff_progress)
            );

            std::print("\rStage {}/{} progress: {}/{} ({:.3f}%). ETA: {:%H:%M:%S}", stage_idx+1, _stages.size(),
                thousands_separator(current_progress), thousands_separator(total), 100.f * current_progress / total, eta);
	    std::cout << std::flush;

            previous_progress = current_progress;
            previous_stage_time = current_time;

            std::this_thread::sleep_until(current_time + 100ms);
        }

        auto stage_end = std::chrono::high_resolution_clock::now();
        std::println("\nStage {}/{} done in {:%H:%M:%S}. {} seeds passed.", 
            stage_idx+1, _stages.size(), std::chrono::duration_cast<std::chrono::seconds>(stage_end - stage_start), _top_seeds.size());

        for (auto& thread : threads) {
            if (thread.joinable()) thread.join();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::println("All stages done in {:%H:%M:%S}. Outputing {} seeds.", 
        std::chrono::duration_cast<std::chrono::seconds>(end - start), _top_seeds.size());

    std::vector<Seed<SeedCache>> reverse;
    reverse.reserve(_top_seeds.size());
    while (!_top_seeds.empty()) {
        reverse.emplace_back(std::move(_top_seeds.get_pop()));
    }

    uint64_t rank = 0;
    std::ofstream out(out_path);
    out << "rank;seed;score;water scale;water coverage;elevation type" << std::endl;
    for (auto it = reverse.rbegin(); it != reverse.rend(); it++) {
        std::string elevation_type;
        if (it->elevation_type == ELEVATION_2_0) {
            elevation_type = "2.0";
        } else if (it->elevation_type == ELEVATION_1_1) {
            elevation_type = "1.1";
        } else {
            elevation_type = "island";
        }

        out << ++rank << ";"
            << it->seed << ";"
            << it->score << ";"
            << _water_scales[it->water_scale_idx] << ";"
            << _water_coverages[it->water_coverage_idx] << ";"
            << elevation_type << std::endl;
    }

    return 0;
}

template<typename SeedCache>
void Finder<SeedCache>::worker_first_stage(
    int id, const Stage& stage, const std::vector<NoisePrecompute>& precomputes,
    bool check_twin_seeds, bool check_water_settings, bool check_elevation_types, std::atomic<uint64_t>& progress
) {
    const uint32_t factor = check_twin_seeds ? 1 : 2;
    const uint32_t first_seed = id * factor + _run_options.first_stage_first_seed;
    const uint32_t seed_span = _run_options.threads * factor;

    MapGenSettings map_gen_settings = _map_gen_settings;
    NoiseCache noise_cache;
    void* stage_cache = stage.cache_factory();

    size_t max_scale = check_water_settings ? _water_scales.size() : 1;
    size_t max_coverage = check_water_settings ? _water_coverages.size() : 1;

    for (uint64_t seed64 = first_seed; seed64 < (uint64_t)_run_options.first_stage_last_seed; seed64 += seed_span) {
        uint32_t seed = (uint32_t)seed64;

        for (size_t scale = 0; scale < max_scale; scale++) {
            for (size_t coverage = 0; coverage < max_coverage; coverage++) {
                auto check_type = [&](ElevationType elevation_type) {
                    const NoisePrecompute& precompute = precomputes[scale * _water_coverages.size() + coverage];
                    map_gen_settings.water_scale = _water_scales[scale];
                    map_gen_settings.water_coverage = _water_coverages[coverage];
                    map_gen_settings.elevation_type = elevation_type;

                    if constexpr (std::is_void_v<SeedCache>) {
                        auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, seed, nullptr, stage_cache);
                        if (!results.eliminate) _top_seeds.insert({ seed, elevation_type, scale, coverage, results.score });
                    } else {
                        SeedCache seed_cache;
                        auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, seed, &seed_cache, stage_cache);
                        if (!results.eliminate) _top_seeds.insert({ seed, elevation_type, scale, coverage, results.score, seed_cache});
                    }

                    progress += factor;
                };

                if (check_elevation_types) {
                    for (ElevationType elevation_type : _elevation_types) {
                        check_type(elevation_type);
                    }
                } else {
                    check_type(ELEVATION_2_0);
                }
            }
        }
    }

    stage.cache_dealloc(stage_cache);
}

template<typename SeedCache>
void Finder<SeedCache>::worker_other_stages(
    int, const Stage& stage, const std::vector<NoisePrecompute>& precomputes,
    bool check_twin_seeds, bool check_water_settings, bool check_elevation_types, std::atomic<uint64_t>& progress
) {
    MapGenSettings map_gen_settings = _map_gen_settings;
    NoiseCache noise_cache;
    void* stage_cache = stage.cache_factory();

    auto do_seed = [&](Seed<SeedCache> s) {
        map_gen_settings.water_scale = _water_scales[s.water_scale_idx];
        map_gen_settings.water_coverage = _water_coverages[s.water_coverage_idx];
        map_gen_settings.elevation_type = s.elevation_type;
        const NoisePrecompute& precompute = precomputes[s.water_scale_idx*_water_coverages.size() + s.water_coverage_idx];

        if constexpr (std::is_void_v<SeedCache>) {
            auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, s.seed, nullptr, stage_cache);
            if (!results.eliminate) _top_seeds.insert({ s.seed, s.elevation_type, s.water_scale_idx, s.water_coverage_idx, results.score });
        } else {
            SeedCache seed_cache = s.cache;
            auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, s.seed, &seed_cache, stage_cache);
            if (!results.eliminate) _top_seeds.insert({ s.seed, s.elevation_type, s.water_scale_idx, s.water_coverage_idx, results.score, seed_cache});
        }

        progress++;

        if (check_twin_seeds) {
            s.seed++;

            if constexpr (std::is_void_v<SeedCache>) {
                auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, s.seed, nullptr, stage_cache);
                if (!results.eliminate) _top_seeds.insert({ s.seed, s.elevation_type, s.water_scale_idx, s.water_coverage_idx, results.score });
            } else {
                auto results = stage.eval_function(map_gen_settings, precompute, noise_cache, s.seed, &s.cache, stage_cache);
                if (!results.eliminate) _top_seeds.insert({ s.seed, s.elevation_type, s.water_scale_idx, s.water_coverage_idx, results.score, s.cache});
            }

            progress++;
        }
    };

    while (!_previous_top_seeds.empty()) {
        Seed<SeedCache> s = _previous_top_seeds.get_pop();
        if (check_water_settings && check_elevation_types) {
            for (size_t scale = 0; scale < _water_scales.size(); scale++) {
                for (size_t coverage = 0; coverage < _water_coverages.size(); coverage++) {
                    for (ElevationType elevation_type : _elevation_types) {
                        s.water_scale_idx = scale;
                        s.water_coverage_idx = coverage;
                        s.elevation_type = elevation_type;
                        do_seed(s);
                    }
                }
            }
            continue;
        }

        if (check_water_settings) {
            for (size_t scale = 0; scale < _water_scales.size(); scale++) {
                for (size_t coverage = 0; coverage < _water_coverages.size(); coverage++) {
                    s.water_scale_idx = scale;
                    s.water_coverage_idx = coverage;
                    do_seed(s);
                }
            }
            continue;
        }

        if (check_elevation_types) {
            for (ElevationType elevation_type : _elevation_types) {
                s.elevation_type = elevation_type;
                do_seed(s);
            }
            continue;
        }

        do_seed(s);
    }

    stage.cache_dealloc(stage_cache);
}
