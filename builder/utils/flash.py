# Copyright (c) Kuba Szczodrzyński 2022-06-12.

from SCons.Script import DefaultEnvironment, Environment

env: Environment = DefaultEnvironment()


def env_add_flash_layout(env: Environment, board):
    flash_layout: dict = board.get("flash")
    if flash_layout:
        defines = {}
        flash_size = 0
        fal_items = ""
        # add "root" partition
        fal_items += "FAL_PART_TABLE_ITEM(root,ROOT)"
        # add all partitions
        for name, layout in flash_layout.items():
            name = name.upper()
            (offset, _, length) = layout.partition("+")
            offset = int(offset, 16)
            length = int(length, 16)
            defines[f"FLASH_{name}_OFFSET"] = f"0x{offset:06X}"
            defines[f"FLASH_{name}_LENGTH"] = f"0x{length:06X}"
            fal_items += f"FAL_PART_TABLE_ITEM({name.lower()},{name})"
            flash_size = max(flash_size, offset + length)
        defines["FLASH_LENGTH"] = f"0x{flash_size:06X}"
        # for "root" partition
        defines["FLASH_ROOT_OFFSET"] = "0x000000"
        defines["FLASH_ROOT_LENGTH"] = f"0x{flash_size:06X}"
        # add partition table array
        defines["FAL_PART_TABLE"] = "{" + fal_items + "}"
        env.Replace(FLASH_DEFINES=defines)
        env.Replace(**defines)


env.AddMethod(env_add_flash_layout, "AddFlashLayout")
