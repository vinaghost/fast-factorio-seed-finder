function remove_proto (excluded_prototypes)
    for _, resource in pairs(data.raw["resource"]) do
        resource.map_grid = false
    end

    data.raw["resource"]["iron-ore"].map_color = { 0, 0, 255 }
    data.raw["resource"]["copper-ore"].map_color = { 255, 0, 0 }
    data.raw["resource"]["coal"].map_color = { 0, 0, 0 }
    data.raw["resource"]["stone"].map_color = { 128, 128, 128 }
    data.raw["tile"]["water"].map_color = { 0, 0, 0 }
    data.raw["tile"]["grass-1"].map_color = { 255, 255, 255 }

    local gen_settings = data.raw["planet"]["nauvis"].map_gen_settings
    gen_settings.cliff_settings = nil

    local autoplace_settings = gen_settings.autoplace_settings
    for name, _ in pairs(autoplace_settings.entity.settings) do
        if excluded_prototypes[name] then goto continue end
        autoplace_settings.entity.settings[name] = nil
        ::continue::
    end
    for name, _ in pairs(autoplace_settings.tile.settings) do
        if excluded_prototypes[name] then goto continue end
        autoplace_settings.tile.settings[name] = nil
        ::continue::
    end

    for name, _ in pairs(autoplace_settings.decorative.settings) do
        autoplace_settings.decorative.settings[name] = nil
    end

    gen_settings.autoplace_settings.entity.settings["fish"] = nil
end

remove_proto({
    ["iron-ore"] = true,
    ["copper-ore"] = true,
    ["coal"] = true,
    ["stone"] = true,
})

data.raw["noise-function"]["resource_autoplace_all_patches"].expression = "regular_patches"

data.raw["noise-function"]["resource_autoplace_all_patches"].local_expressions.regular_patches = "spot_noise{x = x,\z
                                    y = y,\z
                                    density_expression = regular_density_at(distance),\z
                                    spot_quantity_expression = regular_spot_quantity_expression,\z
                                    spot_radius_expression = min(32, regular_rq_factor * regular_spot_quantity_expression ^ (1/3)),\z
                                    spot_favorability_expression = 1,\z
                                    seed0 = map_seed,\z
                                    seed1 = seed1,\z
                                    region_size = 1024,\z
                                    candidate_spot_count = candidate_spot_count,\z
                                    suggested_minimum_candidate_point_spacing = 45.254833995939045,\z
                                    skip_span = regular_patch_set_count,\z
                                    skip_offset = regular_patch_set_index,\z
                                    hard_region_target_quantity = 0,\z
                                    basement_value = basement_value,\z
                                    maximum_spot_basement_radius = 128}"
-- data.raw["noise-function"]["resource_autoplace_all_patches"].local_expressions.regular_spot_quantity_expression = "1000000"

-- data.raw["noise-expression"]["elevation"].expression = "\
-- variable_persistence_multioctave_noise{\
--     x = 10,\
--     y = 30,\
--     persistence = 2,\
--     seed0 = 250,\
--     seed1 = 22,\
--     octaves = 4,\
--     input_scale = 0.16,\
--     output_scale = 5,\
--     offset_x = 300,\
--     offset_y = 200\
-- }"

-- data.raw["noise-expression"]["elevation"].expression = "\
-- quick_multioctave_noise{\
--     x = x,\
--     y = y,\
--     seed0 = 250,\
--     seed1 = 22,\
--     octaves = 4,\
--     input_scale = 0.16,\
--     output_scale = 10,\
--     offset_x = 300,\
--     offset_y = 200,\
--     octave_input_scale_multiplier = 0.5,\
--     octave_output_scale_multiplier = 2,\
--     octave_seed0_shift = 11\
-- } + 5"



-- data.raw["noise-expression"]["elevation"].expression = "\
-- variable_persistence_multioctave_noise{\
--     x = x,\
--     y = y,\
--     persistence = 0.4,\
--     seed0 = 250,\
--     seed1 = 22,\
--     octaves = 4,\
--     input_scale = 0.16,\
--     output_scale = 10,\
--     offset_x = 300,\
--     offset_y = 200\
-- } + 5"

-- data.raw["noise-expression"]["elevation"].expression = "\
-- variable_persistence_multioctave_noise{\
--     x = x,\
--     y = y,\
--     persistence = 0.1,\
--     seed0 = 250,\
--     seed1 = 22,\
--     octaves = 1,\
--     input_scale = 0.02,\
--     output_scale = 0.5,\
--     offset_x = 300,\
--     offset_y = 200\
-- } - basis_noise{\
--     x = x,\
--     y = y,\
--     seed0 = 250,\
--     seed1 = 22,\
--     input_scale = 0.01,\
--     output_scale = 1,\
--     offset_x = 300,\
--     offset_y = 200\
-- }+0"

-- data.raw["noise-expression"]["elevation"].expression = "\
-- multioctave_noise{\
--     x = 20,\
--     y = 10,\
--     persistence = 0.4,\
--     seed0 = 250,\
--     seed1 = 22,\
--     octaves = 4,\
--     input_scale = 0.02,\
--     output_scale = 10,\
--     offset_x = 300,\
--     offset_y = 200\
-- } + 5"

--run --generate-map-preview "/home/ness056/Desktop/test.png" --map-gen-seed 1000 --mod-directory "/home/ness056/CodeProjets/factorio-reverse-engineering/mods" --map-gen-settings "/home/ness056/CodeProjets/factorio-reverse-engineering/map-gen-settings.json" --threads 1
