let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/projects/AmnesiaTheDarkDescent
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +1 HPL2/external/The-Forge.cmake
badd +1 ~/projects/AmnesiaTheDarkDescent
badd +6 term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash
badd +129 amnesia/src/game/LuxEffectRenderer.cpp
badd +15 HPL2/core/resource/shaders/dds_outline.vert.fsl
badd +339 HPL2/core/resource/shaders/ShaderList.fsl
badd +132 amnesia/src/game/LuxEffectRenderer.h
badd +1 HPL2/core/resource/shaders/dds_outline_alpha_reject.frag.fsl653\ Sun\ 21\ May\ 2023\ 12:04:20\ PM\ PDT
badd +2 HPL2/core/resource/shaders/dds_enemy_glow.vert.fsl
badd +14 HPL2/core/resource/shaders/dds_outline_stencil.vert.fsl
badd +12 HPL2/core/resource/shaders/dds_outline_stencil.frag.fsl
badd +13 HPL2/core/resource/fs_dds_outline.sc
badd +83 HPL2/core/resource/shaders/deferred_resources.h.fsl
badd +5 HPL2/core/resource/shaders/dds_outline.frag.fsl
badd +38 HPL2/core/sources/graphics/Material.cpp
badd +91 HPL2/core/include/graphics/Material.h
badd +8 HPL2/core/resource/shaders/material_resource.h.fsl
badd +23 amnesia/src/game/LuxEffectRenderer.cppg
badd +12 HPL2/core/resource/shaders/dds_enemy_glow.frag.fsl
badd +2575 HPL2/core/sources/graphics/RendererDeferred.cpp
badd +490 HPL2/core/include/graphics/RendererDeferred.h
badd +1 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Graphics.cpp
badd +1266 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Interfaces/IGraphics.h
badd +141 amnesia/src/game/LuxEffectHandler.cpp
badd +32 amnesia/src/game/LuxPostEffects.h
badd +33 amnesia/src/game/LuxPostEffects.cpp
badd +200 HPL2/core/sources/graphics/ForgeHandles.cpp
badd +145 HPL2/core/include/graphics/ForgeRenderer.h
badd +187 HPL2/core/include/graphics/ForgeHandles.h
badd +273 amnesia/src/game/LuxInsanityHandler.h
badd +72 amnesia/src/game/LuxInsanityHandler.cpp
badd +18 HPL2/core/resource/fs_dds_posteffect_insanity.sc
badd +1 HPL2/core/resource/shaders/fs_dds_posteffect_insanity.frag.fsl
badd +29 HPL2/core/resource/shaders/dds_insanity_posteffect.frag.fsl
badd +10 HPL2/core/resource/shaders/post_processing_copy.frag.fsl
badd +8 HPL2/core/resource/shaders/copy_channel_4.comp.fsl
badd +17 HPL2/core/resource/shaders/dds_insanity_posteffect.comp.fsl
badd +186 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Tools/ForgeShadingLanguage/includes/vulkan.h
badd +120 HPL2/external/The-Forge.cmake/The-Forge/Examples_3/Visibility_Buffer/src/Shaders/FSL/HDAO.frag.fsl
badd +6 HPL2/core/resource/shaders/solid_z.frag.fsl
badd +127 HPL2/core/include/graphics/Image.h
badd +100 HPL2/core/include/graphics/PostEffect.h
badd +1 HPL2/core/include/graphics/PostEffect_ImageTrail.h
badd +1 HPL2/core/include/resources/TextureManager.h
badd +126 HPL2/core/sources/gui/GuiSet.cpp
badd +154 HPL2/core/sources/resources/TextureManager.cpp
badd +167 amnesia/src/game/LuxInventory.cpp
badd +430 amnesia/src/game/LuxInventory.h
badd +1924 amnesia/src/game/LuxJournal.cpp
badd +315 amnesia/src/game/LuxJournal.h
badd +1615 amnesia/src/game/LuxMainMenu.cpp
badd +1 RendererDeferred.cpp:1168
badd +1 HPL2/core/sources/graphics/ForgeRenderer.cpp
badd +5 HPL2/core/include/FixPreprocessor.h
badd +647 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Vulkan/Vulkan.cpp
badd +1 HPL2/core/include/graphics/Image.h:108:61:
badd +9 HPL2/core/resource/vs_null.sc
badd +1 HPL2/core/include/graphics/Image.h:108:61
badd +1 HPL2/core/include/graphics/Image.h:108
badd +11 HPL2/core/resource/fs_posteffect_blur.sc
badd +1 HPL2/core/resource/shaders/blue_posteffect.frag.fsl
badd +1 HPL2/core/resource/shaders/blur_posteffect.frag.fsl
badd +10 HPL2/external/The-Forge.cmake/cmake/3rdParty/TinyEXR.cmake
badd +33 HPL2/external/The-Forge.cmake/CMakeLists.txt
badd +13 HPL2/external/The-Forge.cmake/cmake/CMakeLists.txt
badd +1 HPL2/external/The-Forge.cmake/cmake/TheForge.cmake
badd +13 HPL2/external/The-Forge.cmake/cmake/Dependencies.cmake
badd +43 HPL2/external/The-Forge.cmake/cmake/The-Forge.cmake
badd +534 ~/projects/The-Forge/Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h
badd +126 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Utilities/RingBuffer.h
badd +2633 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Application/UI/UI.cpp
badd +2213 HPL2/external/The-Forge.cmake/The-Forge/Examples_3/Aura/src/Aura.cpp
badd +242 ~/projects/The-Forge/Common_3/Utilities/RingBuffer.h
badd +67 HPL2/core/sources/system/Bootstrap.cpp
badd +1 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Utilities/Interfaces/IFileSystem.h
badd +71 ~/projects/The-Forge/Common_3/Utilities/Interfaces/IFileSystem.h
badd +5 HPL2/external/The-Forge.cmake/cmake/OS.cmake
badd +10 HPL2/external/The-Forge.cmake/cmake/3rdParty/zstd.cmake
badd +1 HPL2/external/The-Forge.cmake/cmake/3rdParty/volk.cmake
badd +7 HPL2/external/The-Forge.cmake/cmake/3rdParty/astc-encoder.cmake
badd +9 HPL2/external/The-Forge.cmake/cmake/3rdParty/lz4.cmake
badd +1 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Resources/ResourceLoader/ResourceLoader.cpp
badd +1671 ~/projects/The-Forge/Common_3/Graphics/Interfaces/IGraphics.h
badd +503 ~/projects/The-Forge/Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp
badd +282 amnesia/src/game/LuxMainMenu.h
badd +1095 build/bin/Amnesia/Shaders/VULKAN/solid_z.vert
badd +1 HPL2/core/resource/fs_dds_inventory_screen_effect.sc
badd +33 HPL2/core/resource/shaders/dds_inventory_posteffect.frag.fsl
badd +200 HPL2/core/include/gui/GuiSet.h
badd +19 HPL2/core/resource/shaders/gui.frag.fsl
badd +551 HPL2/core/sources/engine/Engine.cpp
badd +322 HPL2/core/include/math/MathTypes.h
badd +193 amnesia/src/game/LuxHelpFuncs.cpp
badd +1 amnesia/src/game/LuxMapHelper.cpp
badd +143 HPL2/core/sources/gui/Widget.cpp
badd +3763 term://~/projects/AmnesiaTheDarkDescent//2585282:/bin/bash
badd +248 amnesia/src/game/LuxLoadScreenHandler.cpp
badd +377 amnesia/src/game/LuxPreMenu.cpp
badd +223 HPL2/core/sources/engine/Updater.cpp
badd +179 amnesia/src/game/LuxCredits.cpp
badd +39 amnesia/src/game/LuxCredits.h
badd +261 HPL2/core/sources/scene/Scene.cpp
argglobal
%argdel
$argadd ~/projects/AmnesiaTheDarkDescent
set stal=2
tabnew +setlocal\ bufhidden=wipe
tabnew +setlocal\ bufhidden=wipe
tabrewind
edit HPL2/external/The-Forge.cmake
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
wincmd _ | wincmd |
split
1wincmd k
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe '3resize ' . ((&lines * 33 + 34) / 68)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
exe '4resize ' . ((&lines * 31 + 34) / 68)
exe 'vert 4resize ' . ((&columns * 92 + 138) / 276)
argglobal
balt HPL2/external/The-Forge.cmake
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1 - ((0 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp | else | edit ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp
endif
balt ~/projects/AmnesiaTheDarkDescent/RendererDeferred.cpp:1168
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1284 - ((9 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1284
normal! 061|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/Image.h", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/Image.h | else | edit ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/Image.h | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/Image.h
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 103 - ((0 * winheight(0) + 16) / 33)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 103
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/HPL2/core/resource/fs_posteffect_blur.sc", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/HPL2/core/resource/fs_posteffect_blur.sc | else | edit ~/projects/AmnesiaTheDarkDescent/HPL2/core/resource/fs_posteffect_blur.sc | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/HPL2/core/resource/fs_posteffect_blur.sc
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/resource/shaders/dds_outline.vert.fsl
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 7 - ((0 * winheight(0) + 15) / 31)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 7
normal! 035|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe '3resize ' . ((&lines * 33 + 34) / 68)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
exe '4resize ' . ((&lines * 31 + 34) / 68)
exe 'vert 4resize ' . ((&columns * 92 + 138) / 276)
tabnext
edit ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.cpp
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
argglobal
if bufexists(fnamemodify("term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash", ":p")) | buffer term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash | else | edit term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash | endif
if &buftype ==# 'terminal'
  silent file term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash
endif
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 6 - ((5 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 6
normal! 064|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/engine/Engine.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1615 - ((45 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1615
normal! 06|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.h", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.h | else | edit ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.h | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.h
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/gui/GuiSet.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 278 - ((28 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 278
normal! 022|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
3wincmd w
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
tabnext
edit ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxMainMenu.cpp
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
argglobal
if bufexists(fnamemodify("term://~/projects/AmnesiaTheDarkDescent//2585282:/bin/bash", ":p")) | buffer term://~/projects/AmnesiaTheDarkDescent//2585282:/bin/bash | else | edit term://~/projects/AmnesiaTheDarkDescent//2585282:/bin/bash | endif
if &buftype ==# 'terminal'
  silent file term://~/projects/AmnesiaTheDarkDescent//2585282:/bin/bash
endif
balt term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 65 - ((64 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 65
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1334 - ((26 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1334
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/RendererDeferred.h", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/RendererDeferred.h | else | edit ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/RendererDeferred.h | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/RendererDeferred.h
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/ForgeHandles.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 490 - ((23 * winheight(0) + 32) / 65)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 490
normal! 045|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
exe 'vert 1resize ' . ((&columns * 92 + 138) / 276)
exe 'vert 2resize ' . ((&columns * 90 + 138) / 276)
exe 'vert 3resize ' . ((&columns * 92 + 138) / 276)
tabnext 2
set stal=1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let &winminheight = s:save_winminheight
let &winminwidth = s:save_winminwidth
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
