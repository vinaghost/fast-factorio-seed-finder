#include "stages.hpp"

int main(int argc, char* argv[]) {
    MapGenSettings settings;

    // Defines what frequencies and sizes should be used for each patch type.
    // In this order: iron, copper, coal, stone, oil.
    settings.frequencies = { 6.f, 6.f, 6.f, 6.f, 6.f };
    settings.sizes = { 6.f, 6.f, 6.f, 6.f, 6.f };

    // Create a finder with a seed cache, see stages.cpp for more details.
    Finder<SeedCache> finder(settings);

    // Defines what water settings should be checked. 
    // Here we set them to each possible values with the in-game map settings slider.
    finder.set_water_scales({ 1.f/6, 0.25f, 1.f/3, 0.5f, 0.75f, 1.f, 4.f/3, 1.5f, 2.f, 3.f, 4.f, 6.f });
    finder.set_water_coverages({ 1.f/6, 0.25f, 1.f/3, 0.5f, 0.75f, 1.f, 4.f/3, 1.5f, 2.f, 3.f, 4.f, 6.f });

    // Add stages.
    finder.add_stage(stage1_eval, stage1_settings);
    finder.add_stage_with_cache<Stage2Cache>(stage2_eval, stage2_settings); // This stage has a stage cache, see stages.cpp for more details.
    finder.add_stage(stage3_eval, stage3_settings);

    return finder.run("tas_any_seed_finder", argc, argv);
}