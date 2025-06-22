#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#undef ERROR
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#ifdef _WIN32
#define VULKAN_SDK_SEARCH_PATH "C:/VulkanSDK"
const char* vulkanSDKPathLIB;
const char* vulkanSDKPathINC;

#define PLATFORM_COMPILER_ARGS vulkanSDKPathINC, "-Wno-deprecated-declarations", "-IC:/ffmpeg/include", "-IC:/freetype/include",
#define PLATFORM_LINKER_FLAGS vulkanSDKPathLIB, "-lvulkan-1", "-lkernel32", "-luser32", "-lgdi32", \
                              "-lshaderc_shared", "-lshaderc_util", "-lglslang", \
                              "-lSPIRV", "-lSPIRV-Tools", "-lSPIRV-Tools-opt", \
                              "-Wno-deprecated-declarations", "-LC:/ffmpeg/lib", "-LC:/freetype/lib",
#else
#define PLATFORM_COMPILER_ARGS
#define PLATFORM_LINKER_FLAGS "-lvulkan", "-lX11", "-lXrandr", "-lshaderc", "-lc", "-lm"
#endif

#define OUTPUT_PROGRAM_NAME "fplayer"
#define COMPILER_ARGS PLATFORM_COMPILER_ARGS "-I./", "-I./src"
#define LINKER_FLAGS PLATFORM_LINKER_FLAGS "-lavcodec", "-lavdevice", "-lavfilter", "-lavformat", "-lavutil", "-lswscale", "-lswresample", "-lfreetype"
#define BUILD_PATH(debug) (debug ? "build/debug/" : "build/release/")

static char* strltrim(char* s) {
    while (isspace(*s)) s++;
    return s;
}

static bool is_dir(const char* path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && 
           (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

static const char* nob_get_ext(const char* path) {
    const char* end = path;
    while (*end) end++;
    while (end >= path) {
        if (*end == '.') return end + 1;
        if (*end == '/' || *end == '\\') break;
        end--;
    }
    return path + strlen(path);
}

static void remove_directory(const char *path) {
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    HANDLE hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_directory(full_path);
        } else {
            DeleteFile(full_path);
        }
    } while (FindNextFile(hFind, &find_data));

    FindClose(hFind);
    RemoveDirectory(path);
#else
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (lstat(full_path, &statbuf) == -1) {
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            remove_directory(full_path);
        } else {
            unlink(full_path);
        }
    }
    closedir(dir);
    rmdir(path);
#endif
}

static void remove_backslashes(char* data) {
    char* backslash;
    while((backslash=strchr(data, '\\'))) {
        switch(backslash[1]) {
        case '\n':
            memmove(backslash, backslash+2, strlen(backslash+2)+1);
            break;
        default:
            memmove(backslash, backslash+1, strlen(backslash+1)+1);
        }
        data=backslash;
    }
}

static bool dep_analyse_str(char* data, char** result, Nob_File_Paths* paths) {
    char* result_end = strchr(data, ':');
    if(!result_end) return false;
    result_end[0] = '\0';
    *result = data;
    data = result_end+1;
    remove_backslashes(data);
    char* lineend;
    if((lineend=strchr(data, '\n')))
        lineend[0] = '\0';
    while((data=(char*)strltrim(data))[0]) {
        char* path=data;
        while(data[0] && data[0] != ' ') data++;
        nob_da_append(paths, path);
        if(data[0]) {
            data[0] = '\0';
            data++;
        }
    }
    return true;
}

static bool nob_c_needs_rebuild(Nob_String_Builder* string_buffer, Nob_File_Paths* paths, 
                               const char* output_path, const char* input_path) {
    string_buffer->count = 0;
    paths->count = 0;
    
    const char* ext = nob_get_ext(output_path);
    char d_file[1024];
    snprintf(d_file, sizeof(d_file), "%.*sd", (int)(ext - output_path), output_path);

    if (nob_needs_rebuild(d_file, &input_path, 1) != 0) {
        return true;
    }

    if (!nob_read_entire_file(d_file, string_buffer)) {
        return true;
    }
    nob_da_append(string_buffer, '\0');
    
    char* obj;
    char* data = string_buffer->items;

    char* current_pos = data;
    while ((current_pos = strchr(current_pos, '\r'))) {
        memmove(current_pos, current_pos + 1, strlen(current_pos + 1) + 1);
    }
    current_pos = data;
    while ((current_pos = strchr(current_pos, '\\'))) {
        if (current_pos[1] != '\r' && current_pos[1] != '\n') {
            *current_pos = '/';
        }
        current_pos++;
    }

    if(!dep_analyse_str(data, &obj, paths)) return true;
    
    return nob_needs_rebuild(output_path, (const char**)paths->items, paths->count) != 0;
}

static String_View get_dirname(const char* path) {
    String_View sv = sv_from_cstr(path);
    if (sv.count == 0) return sv;

    const char* end = path + sv.count - 1;
    while (end > path && *end != '/' && *end != '\\') end--;
    
    sv.count = (end > path) ? (size_t)(end - path) : 0;
    return sv;
}

static bool folder_exists(const char* path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && 
           (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

static void make_needed_folders_recursive(String_View filename) {
    if (filename.count == 0) return;
    if(folder_exists(filename.data)) return;
    String_View sv = filename;

    String_Builder sb = {0};
    while (sv.count > 0) {
        sv_chop_by_delim(&sv, '/');
        sb.count = 0;
        sb_append_buf(&sb, filename.data, sv.data - filename.data);
        sb_append_null(&sb);
        mkdir_if_not_exists(sb.items);
    }
    sb_free(sb);
}

static bool change_extension(String_Builder* sb, const char* filename, const char* new_ext) {
    String_View sv = sv_from_cstr(filename);
    sv_chop_by_delim(&sv, '.');

    if (sv.count == 0) return false;
    sb_append_buf(sb, filename, sv.data - filename);
    sb_append_cstr(sb, new_ext);
    return true;
}

static bool collect_source_files(const char* dirpath, Nob_File_Paths* paths) {
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir(dirpath, &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char* child = children.items[i];
        if (child[0] == '.') continue;

        char* fullpath = nob_temp_sprintf("%s/%s", dirpath, child);

        if (is_dir(fullpath)) {
            if (!collect_source_files(fullpath, paths)) {
                nob_da_free(children);
                return false;
            }
        } else {
            nob_da_append(paths, nob_temp_strdup(fullpath));
        }
    }

    nob_da_free(children);
    return true;
}

static bool build(Nob_Cmd* cmd, const char* filename, bool debug) {
    Nob_String_Builder obj_path = {0};
    sb_append_cstr(&obj_path, BUILD_PATH(debug));
    if (!change_extension(&obj_path, filename, "o")) return false;
    sb_append_null(&obj_path);

    String_View dir = get_dirname(obj_path.items);
    if (dir.count > 0) {
        char dir_str[1024];
        snprintf(dir_str, sizeof(dir_str), "%.*s", (int)dir.count, dir.data);
        make_needed_folders_recursive(sv_from_cstr(dir_str));
    }

    cmd->count = 0;
    nob_cc(cmd);
    nob_cmd_append(cmd, "-c", filename, "-o", obj_path.items, 
               "-MP", "-MMD");
    nob_cmd_append(cmd, COMPILER_ARGS);
    nob_cmd_append(cmd, debug ? "-g" : "-O3");
    if (debug) nob_cmd_append(cmd, "-DDEBUG");

    bool res = nob_cmd_run_sync(*cmd);
    nob_sb_free(obj_path);
    return res;
}

static bool link_files(Nob_Cmd* cmd, const char* output_filename, 
                       Nob_File_Paths* src_paths, bool debug) {
    cmd->count = 0;
    nob_cc(cmd);

    for (size_t i = 0; i < src_paths->count; i++) {
        Nob_String_Builder obj_path = {0};
        sb_append_cstr(&obj_path, BUILD_PATH(debug));
        if (!change_extension(&obj_path, src_paths->items[i], "o")) {
            nob_sb_free(obj_path);
            return false;
        }
        sb_append_null(&obj_path);
        nob_cmd_append(cmd, nob_temp_strdup(obj_path.items));
        nob_sb_free(obj_path);
    }

    nob_cc_output(cmd, output_filename);
    nob_cmd_append(cmd, LINKER_FLAGS);
    if (debug) nob_cmd_append(cmd, "-g");
    #ifdef _WIN32
    else nob_cmd_append(cmd, "-Wl,/subsystem:windows");

    nob_cmd_append(cmd, "assets/fplayer.res");
    #endif
    return nob_cmd_run_sync(*cmd);
}

#ifdef _WIN32
static bool is_version_newer(const char* v1, const char* v2) {
    if(strcmp(v1, ".") == 0 || strcmp(v1, "..") == 0){
        return false;
    }

    if(strcmp(v2, ".") == 0 || strcmp(v2, "..") == 0){
        return true;
    }

    int major1, minor1, patch1;
    int major2, minor2, patch2;
    if (sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1) != 3) return false;
    if (sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2) != 3) return false;

    return (major1 > major2) || 
           (major1 == major2 && minor1 > minor2) ||
           (major1 == major2 && minor1 == minor2 && patch1 > patch2);
}
#endif

static void change_sb_extension(String_Builder* sb, const char* new_ext) {
    // Find the last '.' in the last path component
    size_t i = sb->count;
    while (i > 0) {
        char c = sb->items[i-1];
        if (c == '.') {
            sb->count = i-1;
            break;
        }
        if (c == '/' || c == '\\') {
            break;
        }
        i--;
    }
    sb_append_cstr(sb, ".");
    sb_append_cstr(sb, new_ext);
}

static bool compile_shader(const char* input_path, const char* output_path) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "glslc");
    
    // Add appropriate flags based on shader type
    const char* filename = strrchr(input_path, '/');
    if (!filename) filename = strrchr(input_path, '\\');
    if (!filename) filename = input_path;
    else filename++;

    String_View sv = sv_from_cstr(input_path);
    sv_chop_by_delim(&sv, '.');
    
    if(sv_eq(sv,sv_from_cstr("vert.glsl"))) {
        nob_cmd_append(&cmd, "-fshader-stage=vertex");
    } else if (sv_eq(sv,sv_from_cstr("frag.glsl"))) {
        nob_cmd_append(&cmd, "-fshader-stage=fragment");
    }

    nob_cmd_append(&cmd, input_path, "-o", output_path);
    
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}

static bool build_shaders(bool force_all) {
    Nob_File_Paths shader_files = {0};
    if (!collect_source_files("assets/shaders/compile", &shader_files)) {
        nob_log(NOB_ERROR, "Failed to collect shader files");
        return false;
    }

    if(!folder_exists("assets/shaders/compiled")) mkdir_if_not_exists("assets/shaders/compiled");

    bool all_compiled = true;
    for (size_t i = 0; i < shader_files.count; i++) {
        const char* input_path = shader_files.items[i];

        String_View sv = sv_from_cstr(input_path);
        sv_chop_by_delim(&sv, '.');

        if(sv_eq(sv,sv_from_cstr("glsl"))) continue;

        // Create output path by changing extension to .spv
        Nob_String_Builder output_path = {0};
        sb_append_cstr(&output_path, "assets/shaders/compiled");
        sb_append_cstr(&output_path, strrchr(input_path, '/'));
        change_sb_extension(&output_path, "spv");
        sb_append_null(&output_path);

        // Make sure output directory exists
        String_View dir = get_dirname(output_path.items);
        if (dir.count > 0) {
            char dir_str[1024];
            snprintf(dir_str, sizeof(dir_str), "%.*s", (int)dir.count, dir.data);
            make_needed_folders_recursive(sv_from_cstr(dir_str));
        }

        if (force_all || nob_needs_rebuild1(output_path.items, input_path)) {
            nob_log(NOB_INFO, "Compiling shader: %s", input_path);
            if (!compile_shader(input_path, output_path.items)) {
                nob_log(NOB_ERROR, "Failed to compile shader: %s", input_path);
                all_compiled = false;
            }
        } else {
            nob_log(NOB_INFO, "Shader %s is up to date", input_path);
        }

        nob_sb_free(output_path);
    }

    nob_da_free(shader_files);
    return all_compiled;
}

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

#ifdef _WIN32
    if (is_dir(VULKAN_SDK_SEARCH_PATH)) {
        Nob_File_Paths versions = {0};
        nob_read_entire_dir(VULKAN_SDK_SEARCH_PATH, &versions);
        
        if (versions.count == 0) {
            nob_log(NOB_ERROR, "No VulkanSDK versions found in %s", VULKAN_SDK_SEARCH_PATH);
            return 1;
        }

        const char* latest = versions.items[0];
        for (size_t i = 1; i < versions.count; i++) {
            if (is_version_newer(versions.items[i], latest)) {
                latest = versions.items[i];
            }
        }

        vulkanSDKPathINC = nob_temp_sprintf("-I%s/%s/Include", VULKAN_SDK_SEARCH_PATH, latest);
        vulkanSDKPathLIB = nob_temp_sprintf("-L%s/%s/Lib", VULKAN_SDK_SEARCH_PATH, latest);
        nob_da_free(versions);
    } else {
        nob_log(NOB_ERROR, "VulkanSDK not found at %s", VULKAN_SDK_SEARCH_PATH);
        return 1;
    }
#endif

    bool debug = true;
    bool run_after = false;
    bool clean = false;
    bool shaders_only = false;

    File_Paths argsToPass = {0};
    bool collectingArgs = false;

    char* program = nob_shift_args(&argc, &argv);
    while (argc > 0) {
        char* arg = nob_shift_args(&argc, &argv);
        if(collectingArgs){
            da_append(&argsToPass, arg);
            continue;
        }

        if (strcmp(arg, "release") == 0) debug = false;
        else if (strcmp(arg, "run") == 0) run_after = true;
        else if (strcmp(arg, "clean") == 0) clean = true;
        else if (strcmp(arg, "shaders") == 0) shaders_only = true;
        else if (strcmp(arg, "--") == 0) {
            collectingArgs = true;
            continue;
        }
        else{
            printf("Unknown arg %s\n", arg);
            return 1;
        }
    }

    if (clean) {
        remove_directory("build");
        return 0;
    }

    if (shaders_only){
        return build_shaders(true) ? 0 : 1;
    }

    if(!build_shaders(false)){
        nob_log(NOB_ERROR, "Failed to compile shaders");
        return 1;
    }

    const char* output_filename = nob_temp_sprintf(
        "%s%s%s", 
        BUILD_PATH(debug), 
        OUTPUT_PROGRAM_NAME
#ifdef _WIN32
        ,".exe"
#else
        ,""
#endif
    );

    Nob_File_Paths src_paths = {0};
    if (!collect_source_files("src", &src_paths)) {
        nob_log(NOB_ERROR, "Failed to collect source files");
        return 1;
    }

    Nob_File_Paths c_files = {0};
    for (size_t i = 0; i < src_paths.count; i++) {
        const char* ext = nob_get_ext(src_paths.items[i]);
        if (ext && strcmp(ext, "c") == 0) {
            nob_da_append(&c_files, nob_temp_strdup(src_paths.items[i]));
        }
    }

    bool needs_rebuild = false;
    Nob_Cmd cmd = {0};
    Nob_String_Builder sb = {0};
    Nob_File_Paths deps = {0};

    for (size_t i = 0; i < c_files.count; i++) {
        const char* src_file = c_files.items[i];
        
        Nob_String_Builder obj_path = {0};
        sb_append_cstr(&obj_path, BUILD_PATH(debug));
        if (!change_extension(&obj_path, src_file, "o")) {
            nob_log(NOB_ERROR, "Failed to change extension for %s", src_file);
            return 1;
        }
        sb_append_null(&obj_path);
        const char* obj_file = nob_temp_strdup(obj_path.items);

        if (nob_c_needs_rebuild(&sb, &deps, obj_file, src_file)) {
            needs_rebuild = true;
            if (!build(&cmd, src_file, debug)) {
                nob_log(NOB_ERROR, "Failed to build %s", src_file);
                return 1;
            }
        }
        nob_sb_free(obj_path);
    }

    if (needs_rebuild || !file_exists(output_filename)) {
        if (!link_files(&cmd, output_filename, &c_files, debug)) {
            nob_log(NOB_ERROR, "Linking failed");
            return 1;
        }
    }

    if (run_after) {
        cmd.count = 0;
        nob_cmd_append(&cmd, output_filename);
        if(argsToPass.count > 0){
            da_append_many(&cmd,argsToPass.items, argsToPass.count);
        }
        if (!nob_cmd_run_sync(cmd)) {
            nob_log(NOB_ERROR, "Failed to run the program");
            return 1;
        }
    }

    // Free resources
    nob_da_free(src_paths);
    nob_da_free(c_files);
    nob_da_free(deps);
    nob_sb_free(sb);
    nob_cmd_free(cmd);

    return 0;
}