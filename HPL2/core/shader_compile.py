import argparse
from enum import Enum
import subprocess
import sys
import os

processes = []

parser = argparse.ArgumentParser(description='generate core shaders.')
parser.add_argument('--output', dest='output', action='store', default='core/shaders', help='output directory')
parser.add_argument('--compiler', dest='compiler', action='store', help='shaderc compiler from bgfx')
parser.add_argument('--bgfx', dest='bgfx', action='store', help='bgfx root path for include')

class ShaderType(Enum):
    FS = 0
    VS = 1
    CS = 2

water_variants = [
    {"bit": 1, "defines": ["USE_REFLECTION"]},
    {"bit": 2, "defines": ["USE_FOG"]},
    {"bit": 4, "defines": ["USE_CUBE_MAP_REFLECTION"]},
    {"bit": 8, "defines": ["USE_REFRACTION"]}
]

basic_solid_diffuse_variants = [
    {"bit": 1, "defines": ["USE_NORMAL_MAP"]},
    {"bit": 2, "defines": ["USE_SPECULAR_MAPS"]},
    {"bit": 4, "defines": ["USE_PARALLAX_MAPS"]},
    {"bit": 8, "defines": ["USE_ENVMAP"]}
]

basic_solid_z_variants = [
    {"bit": 1, "defines": ["USE_ALPHA_MAP"]},
    {"bit": 2, "defines": ["USE_DISSOLVE_FILTER"]},
    {"bit": 4, "defines": ["USE_DISSOLVE_ALPHA_MAP"]},
]

translucent_variants = [
    {"bit": 1, "defines": ["USE_DIFFUSE_MAP"]},
    {"bit": 2, "defines": ["USE_NORMAL_MAP"]},
    {"bit": 4, "defines": ["USE_REFRACTION"]},
    {"bit": 8, "defines": ["USE_CUBE_MAP"]},
    {"bit": 16, "defines": ["USE_USE_FOG"]},
]

spotlight_variants = [
    {"bit": 1, "defines": ["USE_GOBO_MAP"]},
    {"bit": 2, "defines": ["USE_SHADOWS"]},
]

pointlight_variants = [
    {"bit": 1, "defines": ["USE_GOBO_MAP"]}
]

deferred_fog_variants = [
    {"bit": 1, "defines": ["USE_OUTSIDE_BOX"]},
    {"bit": 2, "defines": ["USE_BACK_SIDE"]}
]

shaders = [
# Amneisa Specific Shaders compiler can be moved to its own library
    { "type" : ShaderType.VS, "inout" : "resource/vs_dds_enemy_glow.io",              "input": "resource/vs_dds_enemy_glow.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_dds_enemy_glow.io",              "input": "resource/fs_dds_enemy_glow.sc" , "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_dds_flash.io",                   "input": "resource/vs_dds_flash.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_dds_flash.io",                   "input": "resource/fs_dds_flash.sc" , "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_dds_outline.io",                 "input": "resource/vs_dds_outline.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_dds_outline.io",                 "input": "resource/fs_dds_outline.sc" , "includes": ["resource"]},
# deferred
    { "type" : ShaderType.VS, "inout" : "resource/vs_deferred_light.io",              "input": "resource/vs_deferred_light.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_deferred_light.io",              "input": "resource/fs_deferred_pointlight.sc", "includes": ["resource"], "variants": pointlight_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_deferred_light.io",              "input": "resource/fs_deferred_spotlight.sc", "name": "fs_deferred_spotlight_low", "includes": ["resource"], "variants": spotlight_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_deferred_light.io",              "input": "resource/fs_deferred_spotlight.sc", "name": "fs_deferred_spotlight_medium", "includes": ["resource"], "defines": ["SHADOW_JITTER_SIZE=32", "SHADOW_JITTER_SAMPLES=16"], "variants": spotlight_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_deferred_light.io",              "input": "resource/fs_deferred_spotlight.sc", "name": "fs_deferred_spotlight_high", "includes": ["resource"], "defines": ["SHADOW_JITTER_SIZE=64", "SHADOW_JITTER_SAMPLES=32"], "variants": spotlight_variants},
    
    { "type" : ShaderType.VS, "inout" : "resource/vs_light_box.io",                   "input": "resource/vs_light_box.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_light_box.io",                   "input": "resource/fs_light_box.sc", "includes": ["resource"]},
#gui
    { "type" : ShaderType.FS, "inout" : "resource/vs_gui.io",                          "input": "resource/fs_gui.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_gui.io",                          "input": "resource/vs_gui.sc", "includes": ["resource"]},
#  post effects
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_dds_posteffect_insanity.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_dds_inventory_screen_effect.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_fullscreen_fog.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_blur.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_bloom_add.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_image_trail_frag.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_radial_blur_frag.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_posteffect_color_conv.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/fs_post_effect_copy.sc", "includes": ["resource"]},
# other
    { "type" : ShaderType.VS, "inout" : "resource/vs_alpha_reject.io",                 "input": "resource/vs_alpha_reject.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_alpha_reject.io",                 "input": "resource/fs_alpha_reject.sc" , "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_null.io",                         "input": "resource/vs_null.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_null.io",                         "input": "resource/fs_null.sc" , "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_basic_solid_diffuse.io",          "input": "resource/vs_basic_solid_diffuse.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_solid_diffuse.io",          "input": "resource/fs_basic_solid_diffuse.sc", "includes": ["resource"], "variants": basic_solid_diffuse_variants},
    { "type" : ShaderType.VS, "inout" : "resource/vs_basic_solid_illumination.io",     "input": "resource/vs_basic_solid_illumination.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_basic_solid_z.io",                "input": "resource/vs_basic_solid_z.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_decal_material.io",               "input": "resource/vs_decal_material.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_deferred_fog.io",                 "input": "resource/vs_deferred_fog.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_post_effect.io",                  "input": "resource/vs_post_effect.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_water_material.io",               "input": "resource/vs_water_material.sc", "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_simple_flat.io",                  "input": "resource/vs_simple_flat.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_simple_flat.io",                  "input": "resource/fs_simple_flat.sc" , "includes": ["resource"]},
    { "type" : ShaderType.VS, "inout" : "resource/vs_simple_diffuse.io",               "input": "resource/vs_simple_diffuse.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_simple_diffuse.io",               "input": "resource/fs_simple_diffuse.sc" , "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_solid_illumination.io",     "input": "resource/fs_basic_solid_illumination.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_solid_z.io",                "input": "resource/fs_basic_solid_z.sc", "includes": ["resource"], "variants":  basic_solid_z_variants},
    { "type" : ShaderType.VS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/vs_basic_translucent_material.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/fs_basic_translucent_material.sc", "defines": ["USE_BLEND_MODE_ADD"], "name": "fs_basic_translucent_blendModeAdd", "includes": ["resource"], "variants": translucent_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/fs_basic_translucent_material.sc", "defines": ["USE_BLEND_MODE_MUL"], "name": "fs_basic_translucent_blendModeMul", "includes": ["resource"], "variants": translucent_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/fs_basic_translucent_material.sc", "defines": ["USE_BLEND_MODE_MULX2"], "name": "fs_basic_translucent_blendModeMulX2", "includes": ["resource"], "variants": translucent_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/fs_basic_translucent_material.sc", "defines": ["USE_BLEND_MODE_ALPHA"], "name": "fs_basic_translucent_blendModeAlpha", "includes": ["resource"], "variants": translucent_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_basic_translucent_material.io",   "input": "resource/fs_basic_translucent_material.sc", "defines": ["USE_BLEND_MODE_PREMUL_ALPHA"], "name": "fs_basic_translucent_blendModePremulAlpha", "includes": ["resource"], "variants": translucent_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_decal_material.io",               "input": "resource/fs_decal_material.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_deferred_fog.io",                 "input": "resource/fs_deferred_fog.sc", "includes": ["resource"],  "variants": deferred_fog_variants},
    { "type" : ShaderType.FS, "inout" : "resource/vs_simple_diffuse.io",               "input": "resource/fs_simple_diffuse.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_simple_flat.io",                  "input": "resource/fs_simple_flat.sc", "includes": ["resource"]},
    { "type" : ShaderType.FS, "inout" : "resource/vs_water_material.io",               "input": "resource/fs_water_material.sc", "includes": ["resource"], "variants": water_variants},
]

def toD3dPrefix(shaderType):
    if shaderType == ShaderType.FS:
        return "ps"
    elif shaderType == ShaderType.VS:
        return "vs"
    elif shaderType == ShaderType.CS:
        return "cs"
    else:
        raise Exception("Unknown shader type")

def toType(shaderType):
    if shaderType == ShaderType.FS:
        return "fragment"
    elif shaderType == ShaderType.VS:
        return "vertex"
    elif shaderType == ShaderType.CS:
        return "compute"
    else:
        raise Exception("Unknown shader type")

def wait_subprocesses():
    global processes
    for p in processes:
        proc = p["process"]
        print("cmd:", p["cmd"])
        out, err = proc.communicate()
        print(out.decode())
        if(err):
            exit(1)
    processes = []

def wrap_subprocess(*args, **kwargs):
    global processes
    if(len(processes) > 100):
        wait_subprocesses()
    # print(f'cmd: {" ".join(args[0])}')
    # try:
    process = subprocess.Popen(*args, **kwargs,
                           stdout=subprocess.PIPE, 
                           stderr=subprocess.PIPE);
    processes.append({
        "cmd": " ".join(args[0]),
        "process": process
    })
    # except subprocess.CalledProcessError as exc:
    #     print(f'{exc.output.decode("utf-8")}')
    # else:
    #     print("Output: \n{}\n".format(output))

def get_name(shader):
    return shader["name"] if "name" in shader else os.path.splitext(os.path.basename(shader["input"]))[0]

def main():
    args = parser.parse_args()

    os.makedirs(f'{args.output}/shaders/dx9/', exist_ok=True)
    os.makedirs(f'{args.output}/shaders/dx11/', exist_ok=True)
    os.makedirs(f'{args.output}/shaders/osx/', exist_ok=True)
    os.makedirs(f'{args.output}/shaders/glsl/', exist_ok=True)
    os.makedirs(f'{args.output}/shaders/essl/', exist_ok=True)
    os.makedirs(f'{args.output}/shaders/spirv/', exist_ok=True)


    def create_shader(shader, options = {}):
        input_file_path = os.path.abspath(shader["input"])
        name = options["name"] if "name" in options else get_name(shader)
        varying_def_path = os.path.abspath(shader["inout"])
        defines = f'{";".join((shader["defines"] if "defines" in shader else []) + (options["defines"] if "defines" in options else []))}'
        includes = [item for inc in shader["includes"] for item in ['-i', os.path.abspath(inc)]]
        
        if sys.platform == 'win32':
            # dx9
            if not shader["type"] == ShaderType.CS:
                wrap_subprocess([
                    args.compiler,
                    '-f', f'{input_file_path}',
                    '-o', f'{args.output}/shaders/dx9/{name}.bin',
                    '--type', f'{toType(shader["type"])}',
                    '--platform', " windows",
                    '--varyingdef', f'{varying_def_path}',
                    '--profile', f'{toD3dPrefix(shader["type"])}_3_0',
                    '--define', defines,
                    '-O', "3",
                    '-i', f'{args.bgfx}/src',
                    ] + includes)
            
            if not shader["type"] == ShaderType.CS:
                wrap_subprocess([
                    args.compiler,
                    '-f', f'{input_file_path}',
                    '-o', f'{args.output}/shaders/dx11/{name}.bin',
                    '--type', f' {toType(shader["type"])}',
                    '--platform', " windows",
                    '--varyingdef', f'{varying_def_path}',
                    '--profile', f'{toD3dPrefix(shader["type"])}_5_0',
                    '--define', defines,
                    '-O', "3",
                    '-i', f'{args.bgfx}/src',
                    ] + includes)
            else:
                wrap_subprocess([
                    args.compiler,
                    '-f', f'{input_file_path}',
                    '-o', f'{args.output}/shaders/dx11/{name}.bin',
                    '--type', f'{toType(shader["type"])}',
                    '--platform', "windows",
                    '--varyingdef', f'{varying_def_path}',
                    f'--profile {toD3dPrefix(shader["type"])}_5_0',
                    '--define', defines,
                    "-O 1",
                    '-i', f'{args.bgfx}/src',
                    ] + includes)

        if sys.platform == 'darwin':
            wrap_subprocess([
                args.compiler,
                '-f', f'{input_file_path}',
                '-o', f'{args.output}/shaders/osx/{name}.bin',
                '--type', f'{toType(shader["type"])}',
                '--platform', "osx",
                '--varyingdef', f'{varying_def_path}',
                '--profile', f'metal',
                '--define', defines,
                '-O', "3",
                '-i', f'{args.bgfx}/src',
                ] + includes)

        if not shader["type"] == ShaderType.CS:
            wrap_subprocess([
                args.compiler,
                '-f', f'{input_file_path}',
                '-o', f'{args.output}/shaders/essl/{name}.bin',
                '--type', f'{toType(shader["type"])}',
                '--platform ', "android",
                '--define', defines,
                '--varyingdef', f'{varying_def_path}',
                '-i', f'{args.bgfx}/src',
                ] + includes)

        # spirv
        if shader["type"] == ShaderType.CS:
            wrap_subprocess([
                args.compiler,
                '-f', f'{input_file_path}',
                '-o', f'{args.output}/shaders/glsl/{name}.bin',
                '--type', f'{toType(shader["type"])}',
                '--platform', "linux",
                '--define', defines,
                '--varyingdef', f'{varying_def_path}',
                '--profile', f'430',
                '-i', f'{args.bgfx}/src',
                ] + includes)
        else:
            wrap_subprocess([
                args.compiler,
                '-f', f'{input_file_path}',
                '-o', f'{args.output}/shaders/glsl/{name}.bin',
                '--type', f'{toType(shader["type"])}',
                '--platform', "linux",
                '--define', defines,
                '--varyingdef', f'{varying_def_path}',
                '--profile', f'130',
                '-i', f'{args.bgfx}/src',
                ] + includes)
        
        if not shader["type"] == ShaderType.CS:
            wrap_subprocess([
                args.compiler,
                '-f', f'{input_file_path}',
                '-o', f'{args.output}/shaders/spirv/{name}.bin',
                '--type', f'{toType(shader["type"])}',
                '--platform', "linux",
                '--define', defines,
                '--varyingdef', f'{varying_def_path}',
                '--profile', f'spirv',
                '-i', f'{args.bgfx}/src',
                ] + includes)


    for shader in shaders:
        if("variants" in shader):
            variants = shader["variants"]
            num_variants = 0
            for variant in variants:
                num_variants |= variant["bit"]
            for i in range(0, num_variants + 1):
                defines = []
                require_bit = 0
                for variant in variants:
                    if (variant["bit"] & i) > 0:
                        defines += variant["defines"]
                        require_bit |= variant["bit_required"] if "bit_required" in variant else 0
                if((require_bit & i) == require_bit): # only create shader if all required bits are set
                    create_shader(shader, {"defines": defines, "name": f'{get_name(shader)}_{i}'})
                else:
                    print(f'Not creating shader {get_name(shader)}_{i} because not all required bits are set')
        else:
            create_shader(shader)
main()
wait_subprocesses()