/* */

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);

call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs

ClassName = "WPFolder"
ObjectLocation = "<WP_DESKTOP>"
ObjectTitle = "SystemLoad test1"
ObjSetupStr = "DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;ICONFILE="dir"slfld.ico;ICONNFILE=1,"dir"slfld_o.ico"
ObjectId = "<SL_FOLDERTEST1>"
/* call CreateObject; */

if ( SysCreateObject( ClassName,  ObjectTitle,  ObjectLocation, ,
                      'OBJECTID='ObjectId';' || ObjSetupStr, "R" ) ) then
  say "Ok!"
else
  say "Fail"

EXIT


CreateObject:
    len = length(id);
    if (len == 0) then do
        Say 'Error with object "'title'": object ID not given.';
        exit;
    end

    if (left(id, 1) \= '<') then do
        Say 'Error with object "'title'": object ID does not start with "<".';
        exit;
    end

    if (right(id, 1) \= '>') then do
        Say 'Error with object "'title'": object ID does not end with ">".';
        exit;
    end

    len = length(setup);
    if ((len > 0) & (right(setup, 1) \= ';')) then do
        Say 'Error with object "'title'": Setup string "'setup'" does not end in semicolon.';
        exit;
    end

    call charout , 'Creating "'title'" of class "'class'", setup "'setup'"... '
    rc = SysIni("USER", "PM_InstallObject", title||";"||class||";"||target||";RELOCATE", setup||"TITLE="||title||";OBJECTID="||id||";");
    if (rc <> "") then do
        Say 'Warning: object "'title'" of class "'class'" could not be created.'
    end
    else do
        Say "OK"
    end

    id = "";

    return;
