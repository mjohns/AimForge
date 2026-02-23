#!/bin/sh
source_directory="source"
compiled_directory="compiled"
vulkan_version="1.0"
mkdir -p $compiled_directory
rm -f $compiled_directory/*


echo "compiling from $source_directory -> $compiled_directory"
for file in "$source_directory"/*; do
	filename=$(basename "$file")
  if [[ "$filename" =~ ^([^\.]+)\.([^\.]+)\.([^\.]+)$ ]]; then
    middlepart="${BASH_REMATCH[2]}"
		filename="${BASH_REMATCH[1]}.${BASH_REMATCH[2]}"
		shader_language="${BASH_REMATCH[3]}"
    if [[ "$middlepart" == "frag" ]]; then
			echo "frag $filename.$shader_language > $filename.spv"
	    dxc -T ps_6_0 -E main -spirv -fspv-target-env=vulkan$vulkan_version -fvk-use-scalar-layout  -Fo "$compiled_directory/$filename.spv" "$source_directory/$filename.hlsl"
	    # dxc -T ps_6_0 -E main -Fo "$compiled_directory/$filename.dxil" "$source_directory/$filename.hlsl"
			# ./shadercross.bin "$source_directory/$filename.hlsl" -s HLSL -d SPIRV -o "$compiled_directory/$filename.spv"
		elif [[ "$middlepart" == "vert" ]]; then
			echo "vert $filename.$shader_language > $filename.spv"
			dxc -T vs_6_0 -E main -spirv -fspv-target-env=vulkan$vulkan_version -fvk-use-scalar-layout -Fo "$compiled_directory/$filename.spv" "$source_directory/$filename.hlsl"
			# dxc -T vs_6_0 -E main  -Fo "$compiled_directory/$filename.dxil" "$source_directory/$filename.hlsl"
			# ./shadercross.bin "$source_directory/$filename.hlsl" -s HLSL -d SPIRV -o "$compiled_directory/$filename.spv"
    fi
  fi
done
