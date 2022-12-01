maximize_window() {
    if [[ $_IN_WINDOW -eq 1 ]]; then
        ACTIVE_WINDOW=$(xprop -root | grep "_NET_ACTIVE_WINDOW(WINDOW)" | cut -d' ' -f5)
        WM_NAME=$(xprop -id ${ACTIVE_WINDOW} _NET_WM_NAME | cut -d\" -f2)
        while [[ $WM_NAME == Desktop ]]; do
            ACTIVE_WINDOW=$(xprop -root | grep "_NET_ACTIVE_WINDOW(WINDOW)" | cut -d' ' -f5)
            WM_NAME=$(xprop -id ${ACTIVE_WINDOW} _NET_WM_NAME | cut -d\" -f2)
        done

        wmctrl -ir ${ACTIVE_WINDOW} -b add,maximized_horz,maximized_vert
        WM_STATE=$(xprop -id ${ACTIVE_WINDOW} _NET_WM_STATE)
        while [[ $WM_STATE != *_NET_WM_STATE_MAXIMIZED_HORZ* || $WM_STATE != *_NET_WM_STATE_MAXIMIZED_VERT* ]]; do
            wmctrl -ir ${ACTIVE_WINDOW} -b add,maximized_horz,maximized_vert
            WM_STATE=$(xprop -id ${ACTIVE_WINDOW} _NET_WM_STATE)
        done
    fi
}

