# nginx-guess-mime

## Introduction

This module inserts a body filter into the nginx chain that will guess the
MIME type of a file if the default file-extension guessing fails.

The code is commented, so it may be used as an example of a simple nginx module, too.

## Dependencies

nginx-guess-mime uses libmagic to guess the MIME type. Please install
`libmagic-dev` or similar to compile.

## Installation

### nginx
This module is not bundled with nginx. You will have to manually compile nginx
for this module to function:

1. Clone this repository
2. Download nginx source
3. Configure nginx to use this module (e.g. `./configure --add-module=/path/to/nginx-guess-mime`)
4. Build nginx as usual (likely `make`)
5. Install nginx to the correct place (likely `sudo make install`)

### tengine
This module is not bundled with tengine. You will have to manually compile the module
for this module to function. Luckily, tengine has a tool to make it rather easy.

After cloning, just run the dso_tool to install the module to the right place.

```
/path/to/dso_tool --add-module=/path/to/nginx-guess-mime
```

On my system, tengine is installed in `/opt/tengine/sbin`, so I would run `/opt/tengine/sbin/dso_tool`.

See [here](http://tengine.taobao.org/document/dso.html) for more information.

## Usage

This module registers a single directive, `guess_mime`, that can be turned on
or off. To enable it globally, write `guess_mime on;` in the `http` block.
To enable or disable per-location write the appropriate directive and argument.

An example configuration where the same files are served, but on port 9006, guess_mime
is enabled, but on port 9005, guess_mime is not, can be seen below:

```
http {
    guess_mime on;

    server {
        listen 9005;
        root /var/www;
        guess_mime off;
    }

    server {
        listen 9006;
        root /var/www;
    }
}
```

## Bugs and error reports

Bugtracker available here: https://github.com/ohnx/nginx-guess-mime/issues

## See also

* Listing of 3rd-party modules: https://www.nginx.com/resources/wiki/modules/index.html
