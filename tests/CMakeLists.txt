include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(draughts_tests
    test_board.cpp
    test_movegen_maxcapture_on.cpp
    test_movegen_maxcapture_off.cpp
    test_promotion.cpp
    test_flying_kings.cpp
    test_zobrist.cpp
    test_perft.cpp
    test_evaluator.cpp
    test_transposition.cpp
    test_search.cpp
)
target_link_libraries(draughts_tests PRIVATE draughts_core draughts_ai Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(draughts_tests)
