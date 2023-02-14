#pragma once

#include <cstdint>
#include <openssl/sha.h>

namespace SimpicServerLib
{
    /* The header which is sent first and only once on each query. It describes how many manual checks that the user is going to have to make, among other things.*/

    enum class MainHeaderCodes
    {
        Success = 0,
        Failure = 1,
        DirectoryAlreadyActive = 2,
        NoResults = 3,
        UnreasonablyLongPath = 4, 
        UnreasonablyLongMaxHam = 5
    };

    enum class DataTypes
    {
        Image = (1),
        Video = (1 << 1),
        Audio = (1 << 2),
        Text = (1 << 3),
        Unspecified = (1 << 4)
    };

    /* The server's reply to the initial client request. */
    struct __attribute__((__packed__)) MainHeader
    {
        uint8_t code; 
        uint8_t _errno;
        uint16_t set_no; 
    };

    /* While the server is in the process of scanning, provide updates.*/
    struct __attribute__((__packed__)) UpdateHeader
    {
        bool done; // The client shouldn't receive updates after this goes to true.
        // these fields explain the number of *total* currently found medias. 
        uint16_t images;
        uint16_t audios;
        uint16_t videos;
        uint16_t texts;
    };

    /* In this set of similar media types to keep, what type are they and how many are there of them, so that the client can process all of this? */
    struct __attribute__((__packed__)) SetHeader
    {
        uint8_t type; // that of a value in DataTypes.
        uint8_t count; 
    };

    /* This is every image. It describes the file extension, the filename, the width, height, and its length in bytes, all of which are very useful to the client. After this header, it will send the picture data in bytes, the length of which being described by length. */
    struct __attribute__((__packed__)) ImageHeader
    {
        // Not null terminated
        char sha256_hash[SHA256_DIGEST_LENGTH];

        uint16_t width;
        uint16_t height;

        /* Image dimensions and file size. */
        uint32_t size;
        uint16_t filename_length;
        uint16_t path_length; 
        // ^^ if this is -1, then there is no path going to be sent (i.e., recursive mode is off)

        /* Client sends a plea, determining whether they want the filename, the path, dimensions, or the file data. Since they may already have it. */

        // IN THIS ORDER, unless it has been disabled by the plea.
        // read/send filename_length bytes for the null-terminated filename
        // read/send path_length bytes for the null-terminated path: not sent regardless if path_length is -1, even if the client makes a plea.

        // read/send size bytes for the whole file data. 
    };

    enum class ClientRequests
    {
        Exit, // Close the connection, no more requests. 
        Scan, // Scan a directory for similar images. 
        ScanRecursive, // Scan recursively in a directory for similar images. 
        Check, // Check if a file or a set of files would be duplicates in a directory.
        CheckRecursive // Check recursively the same thing as above ^^^
    };

    struct __attribute__((__packed__)) ClientRequest
    {
        uint8_t request;
        uint8_t types; // bitwise field for the file types the client wants.
        uint8_t max_ham; // maximum hamming distance that the client is willing to take. 
        uint16_t path_length; // of where to start searching

        // client will send a null-terminated path length. 
    };

    enum class ClientCheckRequestTypes
    {
        ByData, // the client shall send the file data for the server to check.
        ByPath, // the client shall send the path of the file that already exists on the server
        ByPHash // the client shall send the standard perceptual hash for that format.
        // ~~~^ for images, it's the 64-bit perceptual hash from the DCT of the image.
    };

    /* When doing a check, the server needs to be given the file to check, along with its type. */
    /* This will be sent after the initial ClientRequest handshake. */
    /* The client can send an array of ClientCheckRequests, because after the initial ClientRequest*/
    /* handshake, it will send a uint16_t specifying the number of files to check against the path*/
    /* provided by the path in ClientRequest. */
    struct __attribute__((__packed__)) ClientCheckRequest
    {
        uint32_t length; // the length in bytes of the data sent according to
                        // ClientCheckRequestTypes.
        uint8_t type; // what kind of file?
        uint8_t method; // how is the server going to know how to check these files?
        // ~~~^ an enum from ClientCheckRequestTypes
    };

    struct __attribute__((__packed__)) ServerCheckResponse
    {
        uint16_t results; // if -1, the server didn't find anything; otherwise, no. of results
    };

    struct __attribute__((__packed__)) ServerCheckIndividualGenericResponse
    {
        uint16_t index; // this tells you from what index we are talking about
                        // ~~^ referring to the indices given initially by the client to check.
        
        struct SetHeader info; // and now the information of all of the files that conflict.
        // hence forth, treat it as you would a regular scan... the info structure will contain
        // the type and number of subsequent headers that you may plea to receive data or not
    };

    /* A plea containing bitwise flags (abstracted through bitfields) of what the client does not want from the file or whether they want to skip the file entirely. */
    struct __attribute__((__packed__)) ClientPlea
    {
        bool no_data;
        bool skip_file;
    };

    enum class ClientMainPleas
    {
        Continue,
        Stop
    };

    /* A plea for each set of media. This allows the server and client to wait while the client is probably processing the set of images it has. It also allows the client to stop the scan prematurely. */
    struct __attribute__((__packed__)) ClientMainPlea
    {
        uint8_t plea;
    };

    enum class ClientActions
    {
        Keep, // Keep all files. If so, (deprecated: no hashes for deletion) indices will be sent.
        Delete // Delete selected files by their (deprecated: SHA256 hash.) index 
    };

    struct __attribute__((__packed__)) ClientAction
    {
        uint8_t action;
        uint8_t deletions; // should be -1 on ClientActions::Keep
        // an array of indices will then be sent specifying which files should be deleted.
        // the indices should correspond to the order that the files were sent in, 0 indexed.
    };
}