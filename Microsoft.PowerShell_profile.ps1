# Stu's Powershell profile

#
# Aliases
#

New-Alias which get-command

#
# Functions
#

# From https://superuser.com/a/1289962/609934
# Some improvements:
# 1. Use MyInvocation to get the actual command line, including double quotes
# 2. Use *>&1 to redirect all output to the terminal
# 2a. Redirecting wasn't working as expected, but, removing it seems to do what's needed
#     STDERR wasn't being output in some situations
function time {
    $Command = $MyInvocation.Line -Replace ("^$($MyInvocation.MyCommand) ", "")
    Measure-Command { Invoke-Expression $Command | Out-Default }
}

# From https://stackoverflow.com/a/34905638/1352761
function mklink ($target, $link) {
    New-Item -Path $link -ItemType SymbolicLink -Value $target
}


### Later

# From https://stackoverflow.com/a/12143436/1352761
#set-alias cat        get-content
#set-alias cd         set-location
#set-alias clear      clear-host
#set-alias cp         copy-item
#set-alias h          get-history
#set-alias history    get-history
#set-alias kill       stop-process
#set-alias lp         out-printer
#set-alias ls         get-childitem
#set-alias mount      new-mshdrive
#set-alias mv         move-item
#set-alias popd       pop-location
#set-alias ps         get-process
#set-alias pushd      push-location
#set-alias pwd        get-location
#set-alias r          invoke-history
#set-alias rm         remove-item
#set-alias rmdir      remove-item
#set-alias echo       write-output
#
#set-alias cls        clear-host
#set-alias chdir      set-location
#set-alias copy       copy-item
#set-alias del        remove-item
#set-alias dir        get-childitem
#set-alias erase      remove-item
#set-alias move       move-item
#set-alias rd         remove-item
#set-alias ren        rename-item
#set-alias set        set-variable
#set-alias type       get-content
#
#function help
#{
    #get-help $args[0] | out-host -paging
#}
#
#function man
#{
    #get-help $args[0] | out-host -paging
#}
#
#function mkdir
#{
    #new-item -type directory -path $args
#}
#
#function md
#{
    #new-item -type directory -path $args
#}
#
#function prompt
#{
    #"PS " + $(get-location) + "> "
#}
#
#& {
    #for ($i = 0; $i -lt 26; $i++) 
    #{ 
        #$funcname = ([System.Char]($i+65)) + ':'
        #$str = "function global:$funcname { set-location $funcname } " 
        #invoke-expression $str 
    #}
#}
