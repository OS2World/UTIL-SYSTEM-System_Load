#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#define WC_HINT		"SLHint"

// WM_HINT_SET (for hit window) - Show hint with delay.
//   mp1 - Target window handle or NULLHANDLE to tune off the hint.
//   mp2 - Pointer to the hint text or NULL to query text from target window
//         via WM_HINT_GETTEXT message.
#define WM_HINT_SET		WM_USER + 1

// WM_HINT_UPDATE (for hit window) - Update visible hint.
//   mp1 - Target window handle, hint will be updated only if it already showed
//         for this window. Or NULLHANDLE for any target window.
//   mp2 - Same as mp2 for WM_HINT_SET.
#define WM_HINT_UPDATE		WM_USER + 2

// WM_HINT_UNSET (for hit window) - Tune off the hint.
//   mp1 - Target window handle, hint will be hidden only if it already showed
//         for this window. Or NULLHANDLE for any target window.
#define WM_HINT_UNSET		WM_USER + 3

// WM_HINT_GETTEXT (for user window)
//   Hint window sends this message to target window to query hint text.
//   Target window must fill buffer pointed by mp1. The maximum number of
//   characters to store is specified by mp2 (unsigned long).
#define WM_HINT_GETTEXT		WM_USER + 4

HWND hintCreate(HWND hwndOwner);
VOID hintDestroy(HWND hwnd);

