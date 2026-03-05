#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "noise.hpp"

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
class Finder {
public:
    struct EvalResult {
        // If true, score is ignored and the seed will instantly be eliminated.
        // It should be true for as many seeds as possible, in particular during the first stage
        // as inserting seeds into the seed list is not very fast.
        // During the first stage I would say at least 95% of the seeds should be eliminated.
        bool eliminate;
        float score;
    };

    using SeedScorePair = std::pair<uint32_t, float>;
    using EvalFunction = std::function<EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, SeedScorePair)>;
    template<typename T>
    using EvalFunctionWithCache = std::function<EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, SeedScorePair, T& custom_cache)>;

    struct StageSettings {
        // For a given integer n, the regular patches are the exact same for the seeds 2n and 2n+1.
        // So only looking at even seeds is enough check them.
        //
        // True => check all seeds.
        // False => only check even number seeds.
        // If any stage has this set to true, this will true for 
        // every following stages, ignoring the setting.
        bool check_twin_seeds;

        // Defines how many seeds will be passed to the next stage.
        // For the last stage, defines how many seeds will be put in the output file.
        uint32_t seed_nb_to_next_stage;
    };

    Finder(const MapGenSettings& settings) : _map_gen_settings(settings) {}

    /**
     * EvalFunction should return the new score.
     * SeedScorePair contains the seed and its score from the previous stage.
     * NoisePrecompute and NoiseCache may be used by noise functions.
     * 
     * Stages are ran in order of insertion.
     */
    inline void add_stage(EvalFunction eval_function, StageSettings settings) {
        _stages.emplace_back(
            [=](const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& n_cache, SeedScorePair pair, void*) -> EvalResult {
                return eval_function(settings, precompute, n_cache, pair);
            },
            settings,
            [] { return nullptr; },
            [](void*) { return; }
        );
    }
    /**
     * Same as add_stage but the eval function gets a reference to a custom cache.
     * The can be of any type and there is one cache per worker, so it does not need
     * to be thread safe. This cache main use case is if you need some very big array
     * for your eval function, that array should be in the cache, so you don't need
     * to reallocate it every time.
     */
    template<typename CustomCacheType>
    void add_stage_with_cache(EvalFunctionWithCache<CustomCacheType> eval_function, StageSettings settings) {
        _stages.emplace_back(
            [=](const MapGenSettings& settings, const NoisePrecompute& precompute, NoiseCache& n_cache, SeedScorePair pair, void* cache) -> EvalResult {
                return eval_function(settings, precompute, n_cache, pair, *(CustomCacheType*)cache);
            },
            settings,
            [] { return new CustomCacheType; },
            [](void* cache) { delete (CustomCacheType*)cache; }
        );
    }

    int run(std::string program_name, int argc, char* argv[]);

private:
    struct Stage {
        std::function<EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, SeedScorePair, void* custom_cache)> eval_function;
        StageSettings settings;
        std::function<void*()> cache_factory;
        std::function<void(void*)> cache_dealloc;
    };

    void worker_first_stage(int id, const Stage&, const NoisePrecompute&,
        bool check_twin_seeds, std::atomic<uint64_t>& progress);
    void worker_other_stages(int id, const Stage&, const NoisePrecompute&,
        bool check_twin_seeds, std::atomic<uint64_t>& progress);
    
    struct SeedScorePairComparator {
        bool operator()(const SeedScorePair &a, const SeedScorePair &b) const {
            return a.second > b.second;
        }
    };

    struct RunOptions {
        int threads;
        uint32_t first_stage_first_seed;
        uint32_t first_stage_last_seed;
    };

    RunOptions _run_options;
    MapGenSettings _map_gen_settings;

    bool _twin_seeds_already_checked = false;
    std::vector<Stage> _stages;
    TopK<SeedScorePair, SeedScorePairComparator> _previous_top_seeds;
    TopK<SeedScorePair, SeedScorePairComparator> _top_seeds;
};