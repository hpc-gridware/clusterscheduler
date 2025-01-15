# https://github.com/catchorg/Catch2

Include(FetchContent)

FetchContent_Declare(
   Catch2
   GIT_REPOSITORY https://github.com/catchorg/Catch2.git
   GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)