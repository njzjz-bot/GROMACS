#
# This file is part of the GROMACS molecular simulation package.
#
# Copyright (c) 2011,2012,2013,2014,2015 by the GROMACS development team.
# Copyright (c) 2016,2017,2018,2019,2020, by the GROMACS development team, led by
# Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
# and including many others, as listed in the AUTHORS file in the
# top-level source directory and at http://www.gromacs.org.
#
# GROMACS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1
# of the License, or (at your option) any later version.
#
# GROMACS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GROMACS; if not, see
# http://www.gnu.org/licenses, or write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
#
# If you want to redistribute modifications to GROMACS, please
# consider that scientific software is very special. Version
# control is crucial - bugs must be traceable. We will be happy to
# consider code for inclusion in the official distribution, but
# derived work must not be called official GROMACS. Details are found
# in the README & COPYING files - if they are missing, get the
# official version at http://www.gromacs.org.
#
# To help us fund GROMACS development, we humbly ask that you cite
# the research papers on the package. Check out http://www.gromacs.org.

include(CMakeParseArguments)
include(gmxClangCudaUtils)

function (gmx_add_unit_test_library NAME)
    if (GMX_BUILD_UNITTESTS AND BUILD_TESTING)
        add_library(${NAME} STATIC ${UNITTEST_TARGET_OPTIONS} ${ARGN})
        gmx_target_compile_options(${NAME})
        target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H)
        target_include_directories(${NAME} SYSTEM BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/src/external/thread_mpi/include)
        target_link_libraries(${NAME} PRIVATE testutils gmock)
        if(GMX_CLANG_TIDY)
            set_target_properties(${NAME} PROPERTIES CXX_CLANG_TIDY
                "${CLANG_TIDY_EXE};-warnings-as-errors=*;-header-filter=.*")
        endif()
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "7")
            target_compile_options(${NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Weverything ${IGNORED_CLANG_ALL_WARNINGS} -Wno-gnu-zero-variadic-macro-arguments -Wno-zero-as-null-pointer-constant -Wno-missing-variable-declarations>)
        endif()

    endif()
endfunction ()

# This function creates a GoogleTest test executable for a module.  It
# hides all the complexity of how to treat different source files
# under different configuration conditions. It should be extended
# if we ever support another GPU compilation approach.
#
# It can be called with extra options and arguments:
#   MPI
#     To trigger the ctest runner to run this test with multiple ranks
#   HARDWARE_DETECTION
#     To trigger the test executable setup code to run hardware detection
#   CPP_SOURCE_FILES          file1.cpp file2.cpp ...
#     All the normal C++ .cpp source files
#   GPU_CPP_SOURCE_FILES  file1.cpp file2.cpp ...
#     All the C++ .cpp source files that are always needed, but must be
#     compiled in the way that suits GMX_GPU.
#   CUDA_CU_SOURCE_FILES      file1.cu  file2.cu  ...
#     All the normal CUDA .cu source files
#   OPENCL_CPP_SOURCE_FILES   file1.cpp file2.cpp ...
#     All the other C++ .cpp source files needed only with OpenCL
#   SYCL_CPP_SOURCE_FILES   file1.cpp file2.cpp ...
#     All the C++ .cpp source files needed only with SYCL
#   NON_GPU_CPP_SOURCE_FILES  file1.cpp file2.cpp ...
#     All the other C++ .cpp source files needed only with neither OpenCL nor CUDA nor SYCL
#   OBJECT_LIBRARY_DEPENDENCIES  library1 library2 ...
#     All the object libraries upon which the test depends
function (gmx_add_gtest_executable EXENAME)
    if (GMX_BUILD_UNITTESTS AND BUILD_TESTING)
        set(_options MPI HARDWARE_DETECTION)
        set(_multi_value_keywords
            CPP_SOURCE_FILES
            CUDA_CU_SOURCE_FILES
            GPU_CPP_SOURCE_FILES
            OPENCL_CPP_SOURCE_FILES
            SYCL_CPP_SOURCE_FILES
            NON_GPU_CPP_SOURCE_FILES
            OBJECT_LIBRARY_DEPENDENCIES
            )
        cmake_parse_arguments(ARG "${_options}" "" "${_multi_value_keywords}" ${ARGN})

        file(RELATIVE_PATH _input_files_path ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        set(_temporary_files_path "${CMAKE_CURRENT_BINARY_DIR}/Testing/Temporary")
        file(MAKE_DIRECTORY ${_temporary_files_path})
        # Note that the quotation marks in the next line form part of
        # the defined symbol, so that the macro replacement in the
        # source file is as a string.
        # These are only needed for unittest_main.cpp, but for simplicity used
        # for the whole target (since there may be multiple executables in the
        # same directory, it is not straightforward to use a source file
        # property).
        set(EXTRA_COMPILE_DEFINITIONS
            TEST_DATA_PATH="${_input_files_path}"
            TEST_TEMP_PATH="${_temporary_files_path}")
        if (ARG_MPI)
            list(APPEND EXTRA_COMPILE_DEFINITIONS
                 TEST_USES_MPI=true)
        endif()
        if (ARG_HARDWARE_DETECTION)
            list(APPEND EXTRA_COMPILE_DEFINITIONS
                 TEST_USES_HARDWARE_DETECTION=true)
        endif()

        if (GMX_GPU_CUDA AND NOT GMX_CLANG_CUDA)
            # Work around FindCUDA that prevents using target_link_libraries()
            # with keywords otherwise...
            set(CUDA_LIBRARIES PRIVATE ${CUDA_LIBRARIES})
            cuda_add_executable(${EXENAME} ${UNITTEST_TARGET_OPTIONS}
                ${ARG_CPP_SOURCE_FILES}
                ${ARG_CUDA_CU_SOURCE_FILES}
                ${ARG_GPU_CPP_SOURCE_FILES})
        else()
            add_executable(${EXENAME} ${UNITTEST_TARGET_OPTIONS}
                ${ARG_CPP_SOURCE_FILES})
        endif()

        if (GMX_GPU_CUDA)
            if (GMX_CLANG_CUDA)
                target_sources(${EXENAME} PRIVATE
                    ${ARG_CUDA_CU_SOURCE_FILES}
                    ${ARG_GPU_CPP_SOURCE_FILES})
                set_source_files_properties(${ARG_GPU_CPP_SOURCE_FILES} PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ)
                gmx_compile_cuda_file_with_clang(${ARG_CUDA_CU_SOURCE_FILES})
                gmx_compile_cuda_file_with_clang(${ARG_GPU_CPP_SOURCE_FILES})
                if(ARG_CUDA_CU_SOURCE_FILES OR ARG_GPU_CPP_SOURCE_FILES)
                    target_link_libraries(${EXENAME} PRIVATE ${GMX_EXTRA_LIBRARIES})
                endif()
            endif()
        elseif (GMX_GPU_OPENCL)
            target_sources(${EXENAME} PRIVATE ${ARG_OPENCL_CPP_SOURCE_FILES} ${ARG_GPU_CPP_SOURCE_FILES})
            if(ARG_OPENCL_CPP_SOURCE_FILES OR ARG_GPU_CPP_SOURCE_FILES)
                target_link_libraries(${EXENAME} PRIVATE ${OpenCL_LIBRARIES})
            endif()
        elseif (GMX_GPU_SYCL)
            target_sources(${EXENAME} PRIVATE ${ARG_SYCL_CPP_SOURCE_FILES} ${ARG_GPU_CPP_SOURCE_FILES})
            set_source_files_properties(${ARG_GPU_CPP_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${SYCL_CXX_FLAGS}")
            set_source_files_properties(${ARG_SYCL_CPP_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${SYCL_CXX_FLAGS}")
            if(ARG_SYCL_CPP_SOURCE_FILES OR ARG_GPU_CPP_SOURCE_FILES)
                target_link_libraries(${EXENAME} PRIVATE ${SYCL_CXX_FLAGS})
            endif()
        else()
            target_sources(${EXENAME} PRIVATE ${ARG_NON_GPU_CPP_SOURCE_FILES} ${ARG_GPU_CPP_SOURCE_FILES})
        endif()

        gmx_target_compile_options(${EXENAME})
        target_compile_definitions(${EXENAME} PRIVATE HAVE_CONFIG_H ${EXTRA_COMPILE_DEFINITIONS})
        target_include_directories(${EXENAME} SYSTEM BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/src/external/thread_mpi/include)
        # Permit GROMACS code to include externally developed headers,
        # such as the functionality from the nonstd project that we
        # use for gmx::compat::optional. These are included as system
        # headers so that no warnings are issued from them.
        target_include_directories(${EXENAME} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/external)
        if(CYGWIN)
            # Ensure GoogleTest headers can find POSIX things needed
            target_compile_definitions(${EXENAME} PRIVATE _POSIX_C_SOURCE=200809L)
        endif()

        target_link_libraries(${EXENAME} PRIVATE
            testutils common libgromacs gmock
            ${OBJECT_LIBRARY_DEPENDENCIES}
            ${GMX_COMMON_LIBRARIES} ${GMX_EXE_LINKER_FLAGS})

        if(GMX_CLANG_TIDY)
            set_target_properties(${EXENAME} PROPERTIES CXX_CLANG_TIDY
                "${CLANG_TIDY_EXE};-warnings-as-errors=*;-header-filter=.*")
        endif()
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "7")
            target_compile_options(${EXENAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Weverything ${IGNORED_CLANG_ALL_WARNINGS} -Wno-gnu-zero-variadic-macro-arguments -Wno-zero-as-null-pointer-constant -Wno-missing-variable-declarations>)
        endif()
        # clang-3.6 warns about a number of issues that are not reported by more modern compilers
        # and we know they are not real issues. So we only check that it can compile without error
        # but ignore all warnings.
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION MATCHES "^3\.6")
            target_compile_options(${EXENAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
        endif()
    endif()
endfunction()

# This function can be called with extra options and arguments:
#   OPENMP_THREADS <N>    declares the requirement to run the test binary with N OpenMP
#                           threads (when supported by the build configuration)
#   MPI_RANKS <N>         declares the requirement to run the test binary with N ranks
#   INTEGRATION_TEST      requires the use of the IntegrationTest label in CTest
#   SLOW_TEST             requires the use of the SlowTest label in CTest, and
#                         increase the length of the ctest timeout.
#   IGNORE_LEAKS          Skip some memory safety checks.
#
# TODO When a test case needs it, generalize the MPI_RANKS mechanism so
# that ctest can run the test binary over a range of numbers of MPI
# ranks.
function (gmx_register_gtest_test NAME EXENAME)
    if (GMX_BUILD_UNITTESTS AND BUILD_TESTING)
        set(_options INTEGRATION_TEST SLOW_TEST IGNORE_LEAKS)
        set(_one_value_args MPI_RANKS OPENMP_THREADS)
        cmake_parse_arguments(ARG "${_options}" "${_one_value_args}" "" ${ARGN})
        set(_xml_path ${CMAKE_BINARY_DIR}/Testing/Temporary/${NAME}.xml)
        set(_labels GTest)
        set(_timeout 30)
        if (ARG_INTEGRATION_TEST)
            list(APPEND _labels IntegrationTest)
            # Slow build configurations should have longer timeouts.
            # Both OpenCL (from JIT) and ThreadSanitizer (from how it
            # checks) can take signficantly more time than other
            # configurations.
            if (GMX_GPU_OPENCL)
                set(_timeout 240)
            elseif (${CMAKE_BUILD_TYPE} STREQUAL TSAN)
                set(_timeout 300)
            else()
                set(_timeout 120)
            endif()
        elseif (ARG_SLOW_TEST)
            list(APPEND _labels SlowTest)
            set(_timeout 480)
        else()
            list(APPEND _labels UnitTest)
            gmx_get_test_prefix_cmd(_prefix_cmd)
        endif()
        if (ARG_IGNORE_LEAKS)
            gmx_get_test_prefix_cmd(_prefix_cmd IGNORE_LEAKS)
        endif ()
        set(_cmd ${_prefix_cmd} $<TARGET_FILE:${EXENAME}>)
        if (ARG_OPENMP_THREADS)
            if (GMX_OPENMP)
                list(APPEND _cmd -ntomp ${ARG_OPENMP_THREADS})
            endif()
        endif()
        if (ARG_MPI_RANKS)
            if (NOT GMX_CAN_RUN_MPI_TESTS)
                return()
            endif()
            list(APPEND _labels MpiTest)
            if (GMX_MPI)
                set(_cmd
                    ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${ARG_MPI_RANKS}
                    ${MPIEXEC_PREFLAGS} ${_cmd} ${MPIEXEC_POSTFLAGS})
            elseif (GMX_THREAD_MPI)
                list(APPEND _cmd -ntmpi ${ARG_MPI_RANKS})
            endif()
        endif()
        add_test(NAME ${NAME}
                 COMMAND ${_cmd} --gtest_output=xml:${_xml_path})
        set_tests_properties(${NAME} PROPERTIES LABELS "${_labels}")
        set_tests_properties(${NAME} PROPERTIES TIMEOUT ${_timeout})
        add_dependencies(tests ${EXENAME})
    endif()
endfunction ()

function (gmx_add_unit_test NAME EXENAME)
    gmx_add_gtest_executable(${EXENAME} ${ARGN})
    gmx_register_gtest_test(${NAME} ${EXENAME})
endfunction()

function (gmx_add_mpi_unit_test NAME EXENAME RANKS)
    if (GMX_MPI OR (GMX_THREAD_MPI AND GTEST_IS_THREADSAFE))
        gmx_add_gtest_executable(${EXENAME} MPI ${ARGN})
        gmx_register_gtest_test(${NAME} ${EXENAME} MPI_RANKS ${RANKS})
    endif()
endfunction()
