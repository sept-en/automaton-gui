/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   BIG_A_BLACK_CIRCLE_BACK_1000x1000_png;
    const int            BIG_A_BLACK_CIRCLE_BACK_1000x1000_pngSize = 60500;

    extern const char*   default_config_json;
    const int            default_config_jsonSize = 294;

    extern const char*   king_automaton_abi_json;
    const int            king_automaton_abi_jsonSize = 27596;

    extern const char*   PlayBold_ttf;
    const int            PlayBold_ttfSize = 210704;

    extern const char*   PlayRegular_ttf;
    const int            PlayRegular_ttfSize = 199020;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 5;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
