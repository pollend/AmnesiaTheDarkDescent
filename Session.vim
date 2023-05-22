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
badd +1 ~/projects/AmnesiaTheDarkDescent
badd +10064 term://~/projects/AmnesiaTheDarkDescent//2166997:/bin/bash
badd +12 term://~/projects/AmnesiaTheDarkDescent//2167087:/bin/bash
badd +1 amnesia/src/game/LuxEffectRenderer.cpp
badd +4 HPL2/core/resource/shaders/dds_outline.vert.fsl
badd +319 HPL2/core/resource/shaders/ShaderList.fsl
badd +84 amnesia/src/game/LuxEffectRenderer.h
badd +1 HPL2/core/resource/shaders/dds_outline_alpha_reject.frag.fsl653\ Sun\ 21\ May\ 2023\ 12:04:20\ PM\ PDT
badd +2 HPL2/core/resource/shaders/dds_enemy_glow.vert.fsl
badd +5 HPL2/core/resource/shaders/dds_outline_stencil.vert.fsl
badd +14 HPL2/core/resource/shaders/dds_outline_stencil.frag.fsl
badd +13 HPL2/core/resource/fs_dds_outline.sc
badd +84 HPL2/core/resource/shaders/deferred_resources.h.fsl
badd +16 HPL2/core/resource/shaders/dds_outline.frag.fsl
badd +38 HPL2/core/sources/graphics/Material.cpp
badd +91 HPL2/core/include/graphics/Material.h
badd +8 HPL2/core/resource/shaders/material_resource.h.fsl
badd +23 amnesia/src/game/LuxEffectRenderer.cppg
badd +12 HPL2/core/resource/shaders/dds_enemy_glow.frag.fsl
badd +709 HPL2/core/sources/graphics/RendererDeferred.cpp
badd +76 HPL2/core/include/graphics/RendererDeferred.h
badd +80 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Graphics.cpp
badd +3314 HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Interfaces/IGraphics.h
badd +1 term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash
argglobal
%argdel
$argadd ~/projects/AmnesiaTheDarkDescent
set stal=2
tabnew +setlocal\ bufhidden=wipe
tabnew +setlocal\ bufhidden=wipe
tabrewind
edit amnesia/src/game/LuxEffectRenderer.h
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
exe 'vert 1resize ' . ((&columns * 124 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 124 + 187) / 374)
exe 'vert 3resize ' . ((&columns * 124 + 187) / 374)
argglobal
balt amnesia/src/game/LuxEffectRenderer.cpp
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
let s:l = 84 - ((30 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 84
normal! 071|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp | else | edit ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/external/The-Forge.cmake/The-Forge/Common_3/Graphics/Graphics.cpp
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
let s:l = 210 - ((42 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 210
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp", ":p")) | buffer ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp | else | edit ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp | endif
if &buftype ==# 'terminal'
  silent file ~/projects/AmnesiaTheDarkDescent/HPL2/core/sources/graphics/RendererDeferred.cpp
endif
balt ~/projects/AmnesiaTheDarkDescent/HPL2/core/include/graphics/RendererDeferred.h
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
let s:l = 727 - ((39 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 727
normal! 017|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
exe 'vert 1resize ' . ((&columns * 124 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 124 + 187) / 374)
exe 'vert 3resize ' . ((&columns * 124 + 187) / 374)
tabnext
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
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
exe 'vert 1resize ' . ((&columns * 187 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 186 + 187) / 374)
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
let s:l = 10064 - ((63 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 10064
normal! 070|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
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
let s:l = 10064 - ((63 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 10064
normal! 070|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
exe 'vert 1resize ' . ((&columns * 187 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 186 + 187) / 374)
tabnext
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
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
exe 'vert 1resize ' . ((&columns * 187 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 186 + 187) / 374)
argglobal
if bufexists(fnamemodify("term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash", ":p")) | buffer term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash | else | edit term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash | endif
if &buftype ==# 'terminal'
  silent file term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash
endif
balt ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 17 - ((16 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 17
normal! 084|
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
argglobal
if bufexists(fnamemodify("term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash", ":p")) | buffer term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash | else | edit term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash | endif
if &buftype ==# 'terminal'
  silent file term://~/projects/AmnesiaTheDarkDescent//2225151:/bin/bash
endif
balt ~/projects/AmnesiaTheDarkDescent/amnesia/src/game/LuxEffectRenderer.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
let s:l = 1 - ((0 * winheight(0) + 32) / 64)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1
normal! 0
lcd ~/projects/AmnesiaTheDarkDescent
wincmd w
exe 'vert 1resize ' . ((&columns * 187 + 187) / 374)
exe 'vert 2resize ' . ((&columns * 186 + 187) / 374)
tabnext 3
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
