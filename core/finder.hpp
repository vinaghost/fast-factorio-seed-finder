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
 * and running the expensive criteria on for every seed may take too long.
 * For instance, generating the positions/radii of regular patches is around 10x faster than
 * generating the same for starter patches + there are 72x more configurations of starter patches
 * than regular patches because of water settings and twin seeds. So on my computer
 * generating the regular patches take ~30min but the same with starter patches would take multiple days.
 * Moreover, a full scan with water or the exact shape of patches would probably
 * take more like a month with my computer.
 * We can use this by eliminating as many seeds as possible during the first stage using only
 * regular patches, and then refining the filtering with more precise criteria in later stages.
 */
class Finder {
public:
    struct EvalResult {
        // If true, score is ignored and the seed will instantly be eliminated.
        // It should be true for as many seeds, in particular for the first stage
        // as inserting seeds into the seed list is not very fast.
        // For the first stage I would say at least 95% of the seeds should be eliminated.
        bool eliminate;
        float score;
    };

    using SeedScorePair = std::pair<uint32_t, float>;
    using EvalFunction = std::function<EvalResult(const MapGenSettings&, const NoisePrecompute&, NoiseCache&, SeedScorePair)>;

    struct StageSettings {
        // True => check all seeds.
        // False => only check even number seeds.
        // If any stage has this set to true, this will be set to true for 
        // every following stages, ignoring the setting.
        bool check_twin_seeds;

        // Defines how many seeds will be passed to the next stage.
        // For the last stage, defines how many seeds will be put in the output file.
        uint32_t seed_nb_to_next_stage;
    };

    using Stage = std::pair<EvalFunction, StageSettings>;

    Finder(const MapGenSettings& settings) : _map_gen_settings(settings) {}

    /**
     * EvalFunction should return the new score.
     * SeedScorePair contains the seed and its score from the previous stage.
     * NoisePrecompute and NoiseCache may be used by noise functions.
     * 
     * Stages are ran in order of insertion.
     */
    void add_stage(EvalFunction, StageSettings);

    int run(std::string program_name, int argc, char* argv[]);

private:
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