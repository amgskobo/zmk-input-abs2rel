# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

if (CONFIG_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE)
    zephyr_library()
    zephyr_library_sources(input_processor_absolute_to_relative.c)
endif()

if (CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS)
    # On the app target rather than this module's library: it includes
    # zmk/studio/custom.h, which pulls ZMK's nanopb-generated
    # proto/zmk/custom.pb.h, and that header is only on the app include path.
    target_sources(app PRIVATE input_processor_absolute_to_relative_custom_settings.c)
endif()
