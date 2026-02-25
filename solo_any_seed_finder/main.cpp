#include "stages.hpp"

int main(int argc, char* argv[]) {
    MapGenSettings settings;
    settings.frequencies = { 6.f, 6.f, 6.f, 6.f };
    settings.sizes = { 6.f, 6.f, 6.f, 6.f };

    Finder finder(settings);

    finder.add_stage(stage1_eval, stage1_settings);

    return finder.run("solo_any_seed_finder", argc, argv);
}