// ratings.h - SHADOWS the IE content-ratings API header (v2.1b/inc/ratings.h),
// which protsupp.cpp reaches through ratings.h for PICS rating checks.
//
// Shadowed rather than shimmed, on the same principle as binddoc.h: this is an
// Internet Explorer integration surface (RatingEnable, RatingCheckUserAccess, the
// IObtainRating COM interface) with no macOS counterpart and no place in a native
// client. Declaring the interfaces would create the appearance of ratings support.
//
// The functions are declared, not defined. Anything that actually calls one will
// fail to LINK, which is the outcome we want: it flags code belonging to the IE
// story rather than the app, at the point it is pulled in.
#ifndef NATIVE_SHIM_RATINGS_H
#define NATIVE_SHIM_RATINGS_H

#include "win32types.h"

#define S_RATING_ALLOW      ((HRESULT)0x00000000L)
#define S_RATING_DENY       ((HRESULT)0x00000001L)
#define E_RATING_NOT_FOUND  ((HRESULT)0x80040001L)

typedef void (*RATINGCALLBACK)(DWORD dwUserData, HRESULT hr, LPCTSTR pszRating, void* lpvRatingDetails);

HRESULT RatingEnable(HWND hwndParent, LPCSTR pszUsername, BOOL fEnable);
HRESULT RatingCheckUserAccess(LPCSTR pszUsername, LPCSTR pszURL, LPCSTR pszRatingInfo,
                              void* pData, DWORD cbData, void** ppRatingDetails);
HRESULT RatingAccessDeniedDialog(HWND hDlg, LPCSTR pszUsername, LPCSTR pszContentDescription, void* pRatingDetails);
HRESULT RatingFreeDetails(void* pRatingDetails);
HRESULT RatingObtainCancel(HANDLE hRatingObtainQuery);
HRESULT RatingObtainQuery(LPCTSTR pszTargetUrl, DWORD dwUserData, RATINGCALLBACK fCallback, HANDLE* phRatingObtainQuery);
HRESULT RatingSetupUI(HWND hDlg, LPCSTR pszUsername);
HRESULT RatingEnabledQuery(void);

#endif
