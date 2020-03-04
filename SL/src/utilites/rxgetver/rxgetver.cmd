/*
	Sample for RXGETVER.DLL
*/

/* Load functions */
call RxFuncAdd "gvLoadFuncs", 'RXGETVER', "gvLoadFuncs"
call gvLoadFuncs


Signature = "@#Rexx:1.2.3#@##1## 21.01.2015 BldHost::ru:RUS:123::@@Test RXGETVER"

say "gvParse()"
rc = gvParse( Signature, "ver." )
if rc \= "OK" then
  say rc
else
do
  say "  Vendor:          " || ver._BL_VENDOR
  say "  Revision:        " || ver._BL_REVISION
  say "  Date/Time:       " || ver._BL_DATETIME
  say "  Build Machine:   " || ver._BL_BUILDMACHINE
  say "  ASD Feature ID:  " || ver._BL_ASDFEATUREID
  say "  Language Code:   " || ver._BL_LANGUAGECODE
  say "  Country Code:    " || ver._BL_COUNTRYCODE
  say "  Build:           " || ver._BL_BUILD
  say "  FixPack Version: " || ver._BL_FIXPACKVER
  say "  Description:     " || ver._BL_DESCRIPTION
end

drop ver.
say

say "gvGetFromFile()"
rc = gvGetFromFile( "rxgetver.dll", "ver." )
if rc \= "OK" then
  say rc
else
do
  say "  Vendor:          " || ver._BL_VENDOR
  say "  Revision:        " || ver._BL_REVISION
  say "  Date/Time:       " || ver._BL_DATETIME
  say "  Build Machine:   " || ver._BL_BUILDMACHINE
  say "  ASD Feature ID:  " || ver._BL_ASDFEATUREID
  say "  Language Code:   " || ver._BL_LANGUAGECODE
  say "  Country Code:    " || ver._BL_COUNTRYCODE
  say "  Build:           " || ver._BL_BUILD
  say "  FixPack Version: " || ver._BL_FIXPACKVER
  say "  Description:     " || ver._BL_DESCRIPTION
end
