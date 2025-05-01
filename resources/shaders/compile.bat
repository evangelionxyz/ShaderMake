@echo off
ShaderMake.exe --platform DXIL --binary -O3 -c "Shader.cfg" -o "bin" --compiler "%VULKAN_SDK%\Bin\dxc.exe"  --tRegShift 0  --sRegShift 128  --bRegShift 256 --uRegShift 384 --useAPI
ShaderMake.exe --platform SPIRV --binary -O3 -c "Shader.cfg" -o "bin" --compiler "%VULKAN_SDK%\Bin\dxc.exe"  --tRegShift 0  --sRegShift 128  --bRegShift 256 --uRegShift 384 --useAPI -D SPIRV -D TARGET_VULKAN

PAUSE