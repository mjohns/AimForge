# Requires shadercross CLI installed from SDL_shadercross
for filename in *.vert.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.dxil}"
    fi
done

for filename in *.frag.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../compiled/${filename/.hlsl/.dxil}"
    fi
done
