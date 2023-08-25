scons platform=windows target=template_debug debug_symbols=yes;
cp .\bin\libgodotsteam.windows.template_debug.x86_64.dll ..\godot-FirstPersonStarter-main\addons\godotsteam\win64\godotsteam.debug.x86_64.dll;
cp .\bin\libgodotsteam.windows.template_debug.x86_64.pdb ..\godot-FirstPersonStarter-main\addons\godotsteam\win64\godotsteam.debug.x86_64.pdb;
godot -e --path ..\godot-FirstPersonStarter-main\
