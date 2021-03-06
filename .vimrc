" TODO:
" ( ) Debug "no matching autocommands" when doing ':w' from ':Gdiff' window

" Pathogen for now, Vim 8.0 native management later

" To disable a plugin, add its bundle name to the following list
let g:pathogen_disabled = []
"call add(g:pathogen_disabled, 'conflict-marker.vim')
"call add(g:pathogen_disabled, 'vim-colors-solarized')
"call add(g:pathogen_disabled, 'vim-fugitive')

" The only reason to 'execute' instead of 'call' is so pathogen can disable your
" .vimrc. I never want that, and 'call' is safer. 'execute' is like eval.
call pathogen#infect()

set modeline
set background=dark
set nobackup
set cryptmethod=blowfish2
set diffopt+=vertical
set history=1000
set hlsearch
set incsearch
set scrolloff=5
set showcmd
set tags+=tags;,./tags;
" New items
set wildmenu
"set ttyfast
let mapleader="\\"
nnoremap <silent> <leader><space> :noh<cr>
set formatoptions+=j

" Solarized default values, don't recall why I have them listed out
" let g:solarized_termtrans=0
" let g:solarized_degrade=0
" let g:solarized_bold=1
" let g:solarized_underline=1
" let g:solarized_italic=1
" let g:solarized_termcolors=16
" let g:solarized_contrast="normal"
" let g:solarized_visibility="normal"
" let g:solarized_diffmode="normal"
" let g:solarized_hitrail=0
" let g:solarized_menu=1
colorscheme solarized

" Select the correct shell variant for syntax highlighting
let g:is_bash = 1

" Allow saving of files as sudo when I forgot to start vim using sudo
" These should be commands in their own right I think, not mappings
"cnoremap w!! w !sudo tee % > /dev/null
"cnoremap vhelp rightbelow vertical help

filetype indent off

" Turn off auto-commenting
autocmd FileType * setlocal formatoptions-=c
autocmd FileType * setlocal formatoptions-=r
autocmd FileType * setlocal formatoptions-=o

autocmd Filetype python setlocal expandtab shiftwidth=4 smarttab
autocmd Filetype c setlocal expandtab shiftwidth=4 smarttab

" Only works when it's a bare variable; inside an autogroup it gets set too late and c syntax
" doesn't see it. This should be dynamic.
let c_space_errors=1

autocmd BufNewFile,BufRead .sturc set ft=sh
autocmd BufNewFile,BufRead .vimlocal set ft=vim
autocmd BufNewFile,BufRead patch.* set ft=diff
autocmd BufNewFile,BufRead *.rules set ft=udevrules
autocmd BufNewFile,BufRead *.aidl set ft=java
autocmd BufNewFile,BufRead *.gs set ft=javascript
autocmd BufNewFile,BufRead *.xxd set ft=xxd
autocmd BufNewFile,BufRead syslog{,.[0-9]*,-[0-9]*} set ft=messages
autocmd BufNewFile,BufRead syslog set ft=messages

" Ubuntu turned off file position, globally; turn it back on
" Affected: 14.04 16.04 18.04
autocmd BufReadPost * if line("'\"") > 1 && line("'\"") <= line("$") | exe "normal! g'\"" | endif

" vim -b : edit binary using xxd-format!
augroup Binary
  autocmd!
  autocmd BufReadPre *.bin,*.out,xterm let &bin=1
  autocmd BufReadPost *.bin,*.out,xterm if &bin | silent %!xxd
  autocmd BufReadPost *.bin,*.out,xterm set ft=xxd | endif
  autocmd BufWritePre *.bin,*.out,xterm if &bin | silent %!xxd -r
  autocmd BufWritePre *.bin,*.out,xterm endif
  autocmd BufWritePost *.bin,*.out,xterm if &bin | silent %!xxd
  autocmd BufWritePost *.bin,*.out,xterm set nomod | endif
augroup END

function! s:DiffWithOrig()
  let l:ft = &filetype
  diffthis
  rightbelow vnew | read # | normal! 1Gdd
  diffthis
  "execute "setlocal buftype=nofile bufhidden=wipe nobuflisted noswapfile readonly filetype=" . l:ft
  execute "setlocal buftype=nofile bufhidden=wipe noswapfile readonly filetype=" . l:ft
  "file DiffWithOrig
endfunction
command! DiffOrig call s:DiffWithOrig()
nnoremap <silent> <leader>d :DiffOrig<cr>

" This function causes :diffupdate to not redraw the screen properly in Vim 8!
" Needs debugging
"set diffexpr=DiffW()
function DiffW()
  let opt = ""
  if &diffopt =~ "icase"
    let opt = opt . "-i "
  endif
  if &diffopt =~ "iwhite"
    let opt = opt . "-w -B " " swapped vim's -b with -w
  endif
  silent execute "!diff -a --binary " . opt . v:fname_in . " " . v:fname_new .  " > " . v:fname_out
endfunction

" Always be last, source local vim if exists
silent! source .vimlocal
