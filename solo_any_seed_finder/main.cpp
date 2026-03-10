#include "stages.hpp"

int main(int argc, char* argv[]) {
    MapGenSettings settings;
    settings.frequencies = { 6.f, 6.f, 6.f, 6.f };
    settings.sizes = { 6.f, 6.f, 6.f, 6.f };

    Finder<SeedCache> finder(settings);

    finder.set_water_scales({ 1.f/6, 0.25f, 1.f/3, 0.5f, 0.75f, 1.f, 4.f/3, 1.5f, 2.f, 3.f, 4.f, 6.f });
    finder.set_water_coverages({ 1.f/6, 0.25f, 1.f/3, 0.5f, 0.75f, 1.f, 4.f/3, 1.5f, 2.f, 3.f, 4.f, 6.f });

    finder.add_stage(stage1_eval, stage1_settings);
    finder.add_stage(stage2_eval, stage2_settings);
    finder.add_stage(stage3_eval, stage3_settings);

    return finder.run("solo_any_seed_finder", argc, argv);
}