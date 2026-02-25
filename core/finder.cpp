#include "finder.hpp"

#include <thread>
#include <print>
#include <fstream>
#include <argparse/argparse.hpp>
#include <chrono>

void Finder::add_stage(EvalFunction eval_function, StageSettings settings) {
    _stages.push_back(std::make_pair(eval_function, settings));
}

int Finder::run(std::string program_name, int argc, char* argv[]) {
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

    std::println("Precomputing noise data...");
    NoisePrecompute* precompute = new NoisePrecompute(_map_gen_settings);

    for (size_t stage_idx = 0; stage_idx < _stages.size(); stage_idx++) {
        const auto& stage = _stages[stage_idx];
        const auto& stage_settings = stage.second;

        bool check_twin_seeds = false;
        if (_twin_seeds_already_checked == false && stage_settings.check_twin_seeds == true) {
            check_twin_seeds = true;
            _twin_seeds_already_checked = true;
        }

        std::atomic<uint64_t> progress;
        uint32_t total;
        if (stage_idx == 0) {
            total = ((_run_options.first_stage_last_seed - _run_options.first_stage_first_seed) / 2) * 2;
        } else {
            _previous_top_seeds = std::move(_top_seeds);
            total = _previous_top_seeds.size();
            if (check_twin_seeds) total *= 2;
        }

        _top_seeds.clear();
        _top_seeds.set_max_size(stage_settings.seed_nb_to_next_stage);

        std::vector<std::thread> threads;
        for (int id = 0; id < _run_options.threads; id++) {
            std::thread thread;
            if (stage_idx == 0) {
                thread = std::thread([&, id, check_twin_seeds] {
                    worker_first_stage(id, stage, *precompute, check_twin_seeds, progress);
                });
            } else {
                thread = std::thread([&, id, check_twin_seeds] {
                    worker_other_stages(id, stage, *precompute, check_twin_seeds, progress);
                });
            }
            threads.push_back(std::move(thread));
        }

        uint64_t previous_progress = 0;
        auto start = std::chrono::high_resolution_clock::now();
        auto previous_time = start;
        while (progress.load() < total) {
            uint64_t current_progress = progress.load();
            uint64_t diff_progress = current_progress - previous_progress;
            auto current_time = std::chrono::high_resolution_clock::now();
            auto diff_time = current_time - previous_time;

            using namespace std::chrono_literals;
            auto eta = std::chrono::duration_cast<std::chrono::seconds>(
                diff_progress == 0 ? 0s : (total - current_progress) * (diff_time / diff_progress)
            );

            std::print("\rStage {}/{} progress: {}/{} ({:.3f}%). ETA: {:%H:%M:%S}", stage_idx+1, _stages.size(),
                thousands_separator(current_progress), thousands_separator(total), 100.f * current_progress / total, eta);

            previous_progress = current_progress;
            previous_time = current_time;

            std::this_thread::sleep_for(25ms);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::println("\nStage {}/{} done in {:%H:%M:%S}", 
            stage_idx+1, _stages.size(), std::chrono::duration_cast<std::chrono::seconds>(end - start));

        for (auto& thread : threads) {
            if (thread.joinable()) thread.join();
        }
    }

    std::ofstream out(out_path);
    while (!_top_seeds.empty()) {
        auto pair = _top_seeds.get_pop();
        out << pair.second << "; " << pair.second << std::endl;
    }

    return 0;
}

void Finder::worker_first_stage(
    int id, const Stage& stage, const NoisePrecompute& precompute,
    bool check_twin_seeds, std::atomic<uint64_t>& progress
) {
    const uint32_t factor = check_twin_seeds ? 1 : 2;
    const uint32_t first_seed = id * factor + _run_options.first_stage_first_seed;
    const uint32_t seed_span = _run_options.threads * factor;

    NoiseCache* cache = new NoiseCache;

    for (uint64_t seed64 = first_seed; seed64 < (uint64_t)_run_options.first_stage_last_seed; seed64 += seed_span) {
        uint32_t seed = (uint32_t)seed64;
        auto results = stage.first(_map_gen_settings, precompute, *cache, {seed, 0.f});
        if (!results.eliminate) _top_seeds.insert({seed, results.score});
        progress += factor;
    }

    delete cache;
}

void Finder::worker_other_stages(
    int, const Stage& stage, const NoisePrecompute& precompute,
    bool check_twin_seeds, std::atomic<uint64_t>& progress
) {
    NoiseCache* cache = new NoiseCache;

    while (!_previous_top_seeds.empty()) {
        SeedScorePair pair = _previous_top_seeds.get_pop();

        auto results = stage.first(_map_gen_settings, precompute, *cache, pair);
        if (!results.eliminate) _top_seeds.insert({pair.first, results.score});
        progress++;

        if (check_twin_seeds) {
            pair.first++;

            auto results = stage.first(_map_gen_settings, precompute, *cache, pair);
            if (!results.eliminate) _top_seeds.insert({pair.first, results.score});
            progress++;
        }
    }

    delete cache;
}