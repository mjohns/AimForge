for /r "." %%f in (*.cc) do (
    clang-format -i %%f
)
for /r "." %%f in (*.h) do (
    clang-format -i %%f
)