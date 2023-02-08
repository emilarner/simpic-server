#pragma once

#include <cstdint>
#include <openssl/sha.h>


/*
	The Protocol:

	The client connects to the server, obviously, sending a ClientRequest, indicating whether they want to exit, scan, or scan recursively. Recursive scans are not implemented at the moment, but they'd be fairly trivial to implement, nonetheless. The server receives the path that it shall start scanning in, as given by the ClientRequest structure, which is receives.

	Once the server has collected an array of arrays of files, each of them called a 'set' within the context of Simpic, it will begin to send each one of them, denoting their type... the structure used for this is struct SetHeader, and it shall be sent to the client for every set. Before this, however, the client needs to be aware that the scan has been completed and how many results the server found, which is conveyed through the MainHeader sent by the server.

	For each set, another structure describing the kind of set and the number of entries in the set will be sent. At this moment, only detection for similar images is supported, but the program is modular and abstracted to the extent where implementation of other forms of media will be rather trivial. For every entry, the number of which specified by the set structure, a structure containing its data shall be sent to the client--for images, it's struct ImageHeader. This shall only contain cheap-to-send metadata, however, and the client must do additional work later on.

	After such a structure has been sent to the client, the null-terminated filename and pathnames of the file shall be sent, the lengths of which were specified in the aforementioned structure sent. The client will now send a plea (struct ClientPlea)--it may not want to receive the actual data of the file. If the plea says no data, then the server shall not send the corresponding file data; otherwise, yeah.

	Once all of that is done for the individual image or other piece of media, the server must wait for a main plea (struct ClientMainPlea). This is useful for providing an asynchronous, event-driven model between the client and server, which is useful for the GUI-oriented nature of Simpic. The client can either tell, through the main plea, whether to continue or stop. If continued, then more entries will be sent in the way specified. The client knows how many entries are in a set, so it knows when it will have to read a struct SetHeader, the next time it is sent on the next set, until the SetHeader  

*/

namespace SimpicServerLib
{
    /* The header which is sent first and only once on each query. It describes how many manual checks that the user is going to have to make, among other things.*/

    enum class MainHeaderCodes
    {
        Success = 0,
        Failure = 1,
        DirectoryAlreadyActive = 2,
        NoResults = 3,
        Limits = 4
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
        Exit,
        Scan,
        ScanRecursive
    };

    struct __attribute__((__packed__)) ClientRequest
    {
        uint8_t request;
        uint8_t types; // bitwise field for the file types the client wants.
        uint8_t max_ham; // maximum hamming distance that the client is willing to take. 
        uint16_t path_length;

        // client will send a null-terminated path length. 
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
        Keep, // Keep all files. If so, no hashes for deletion will be sent.
        Delete // Delete selected files by their SHA256 hash. 
    };

    struct __attribute__((__packed__)) ClientAction
    {
        uint8_t action;
        uint8_t deletions;
        // an array of indices will then be sent specifying which files should be deleted.
        // the indices should correspond to the order that the files were sent in, 0 indexed.
    };
}