# Run by every Visual Studio project in a build directory that belongs to
# one branch (see CheckBranch.props.in and the branch check in the
# top-level CMakeLists.txt): it fails the project when another branch is
# checked out, so that Visual Studio, which goes on building after a
# failed ZERO_CHECK, compiles nothing of the other branch into this
# directory.
#
#   cmake -DSOURCE_DIR=<repository> -DBRANCH=<branch of the directory>
#         -DTARGET=<target> -P CheckBranch.cmake

execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${SOURCE_DIR}
  OUTPUT_VARIABLE _branch
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _result
  ERROR_QUIET
)
# without git there is nothing to compare; the configure step has
# checked the branch when git was there
if (NOT _result EQUAL 0)
  return()
endif()

# the build scripts flatten topic/xyz to topic_xyz the same way
string(REPLACE "/" "_" _branch "${_branch}")
if (_branch STREQUAL "HEAD")
  set(_branch "no branch (detached HEAD)")
endif()
if (NOT _branch STREQUAL BRANCH)
  message(FATAL_ERROR
    "${TARGET} is not built: this build directory belongs to the branch "
    "'${BRANCH}', but the checked-out branch is '${_branch}'.")
endif()
