FetchContent_Declare(
        asmjit
        GIT_REPOSITORY https://github.com/asmjit/asmjit.git
        GIT_TAG 3577608cab0bc509f856ebf6e41b2f9d9f71acc4
)

#set(ASMJIT_DIR "${asmjit_SOURCE_DIR}")
set(ASMJIT_STATIC TRUE)

FetchContent_MakeAvailable(asmjit)
