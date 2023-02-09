
# simpic-server
The server daemon for simpic--only on Linux. 

Simpic (similar pictures, but other media types too) and all of its component libraries and programs is a system to find perceptually related images and other types of media **(as of now, only images are supported)**, for organizational purposes. It achieves this by utilizing the perceptual hashes of these images, which it computes through the *phash* library. Thus, *phash* must be credited and is therefore a dependency for anything *simpic* related. pHash's website can be found [here](https://www.phash.org/). Speaking of dependencies, here are the dependencies for the Simpic server:

 - pHash (impossible to compile for Windows, hard to compile for Mac OS)
 - OpenSSL: *libcrypto* and *libssl*
 - libjpeg
 - libffmpeg
 - libtiff
 - libpng

Compiling pHash is notoriously hard: their main GitHub and source repositories don't include the relevant information on how to compile their mysterious--yet very useful--library. For this reason, we recommend building and compiling a more updated repository, found [here](https://github.com/starkdg/phash). Good luck, you'll need it.

To compile simpic_server, first run *make* (assuming you have the environment that can build simpic_server, the dependencies and compilers and all of that). Then, you must run *make install* to install the compiled program and its required shared libraries to the system. It *will* not run without installing the compiled shared libraries, so don't complain if you haven't `make install`'d it. 

Here are the valid arguments to pass to simpic_server, quoted directly from its help menu (which can be accessed by passing -h/--help):

    simpic_server - The server daemon for Simpic.
    USAGE:
    
    -p, --port [PORT]              The port for Simpic to bind on. Default: 20202
    ~~~~~~~^ you don't need to care about this, if not running as a service.
    -f, --force-delete             Don't move to a recycle bin, but completely delete.
    ~~~~~~~^ not recommended.
    -r, --recycle-bin [PATH]       Set the recycle bin somewhere other than the default.
    -c, --cache [PATH]             Change the default directory of the cache.

You may notice command-line arguments instead of a dedicated configuration file for the Simpic server. Our response: simpic_server is not large enough to warrant such a thing, and you should be comfortable with editing the service file to have the command-line arguments that you want.

Why not UNIX sockets? Simpic is not meant to be isolated to one system, but it is supposed to be accessible over a network to scan for related images across computers. Plus, the overhead difference from a TCP socket versus a UNIX socket is not dramatic at all. Port 20202 is a pretty non-intrusive and not widely used port, as well. 

The Simpic server runs on the machine (default port: 20202) which is to scan for related media files, of which is accessible by the Simpic client programs and/or libraries. It is designed this way to allow for scanning of related images on machines that are servers or are not currently being physically used by the user, though simpic_client allows for easy usage on one's local machine. It is also useful to have a Simpic server, as to allow for efficient and synchronized caching of perceptual hashes, as to avoid unnecessary computation. Most of all, it provides an abstraction for other applications to scan for related images, with ease and relative efficiency--no matter if the language is interpreted or not. If we want to update the algorithm used in Simpic, we can, since it is idiomatic and the protocol doesn't care about the actual underlying implementation.

Simpic is not really meant to be used on the large scale, and it is not meant to be cross-platform, only being localized to Linux. The algorithm that Simpic uses to find sets of related images, for example, is not really the most optimal, but let us explain it:

 1. Calculate (or retrieve from cache) the perceptual hashes (with pHash, it is a 64-bit integer) of all of the valid image files in a directory.
 2. Generate *non-repeating* two combinations (O(n^2)) of images with two nested for-loops, instances of a class which have their perceptual hashes as well as their SHA256 hashes.
 3. In the second nested layer of the loop, check if the hamming distances of the two perceptual hashes of the images is within an acceptable threshold (the default for the Simpic server is 3). If it is, add it to an std::vector that corresponds to the first layer image, using the main index of the main image/media file from the first loop in an std::unordered_map to reference the std::vector of images.
 4. Store all of the images that were a match throughout the entirety of the second layer loop in an std::unordered_set. On any subsequent iteration of the first loop, we will check if any of the images we're permutating is within our list of 'already gone through items', as to not waste time checking them. Remember, checking whether images are similar is a process that is inherently commutative, meaning that the order we do it in doesn't matter, so this is perfectly valid.
 5. All valid results are simply the values in the std::unordered_map that stores all of the similar images found, if the number of solutions found > 1.

... There's probably a better way, but it works fine now. For a more in-depth view of how the algorithm works (for images) look at the implementation for Image::find_similar_images(std::vector<Image*> &images) in *images.cpp*. You may also test it by running testing/test_simpic_alg, on the dataset of images, which will be placed in dataset/ (this repository already includes them). You may also use the test dataset on pHash's website: [here](https://www.phash.org/download/). 

Technically, simpic_server is compiled as a shared library and then the main entry point program links against it, so that it'd work. If you want to use the functions from simpic_server as a sort of abstraction and as a more C++ friendly version of pHash, go right ahead. Be aware that the relevant header files will be placed in /usr/include/simpic_server/ if one installs simpic_server. 
