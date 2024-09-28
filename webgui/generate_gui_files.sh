#!/bin/bash

get_content_type() {
    local filename="$1"
    local extension="${filename##*.}"
    local mime_type="application/octet-stream"

    case "${extension,,}" in
        html|htm) mime_type="text/html" ;;
        css)      mime_type="text/css" ;;
        js)       mime_type="application/javascript" ;;
        json)     mime_type="application/json" ;;
        xml)      mime_type="application/xml" ;;
        txt)      mime_type="text/plain" ;;
        png)      mime_type="image/png" ;;
        jpg|jpeg) mime_type="image/jpeg" ;;
        gif)      mime_type="image/gif" ;;
        svg)      mime_type="image/svg+xml" ;;
        ico)      mime_type="image/x-icon" ;;
        pdf)      mime_type="application/pdf" ;;
        zip)      mime_type="application/zip" ;;
        gz)       mime_type="application/gzip" ;;
        woff)     mime_type="font/woff" ;;
        woff2)    mime_type="font/woff2" ;;
        ttf)      mime_type="font/ttf" ;;
        otf)      mime_type="font/otf" ;;
        *)        mime_type="application/octet-stream" ;;
    esac

    echo "$mime_type"
}



cd src
find . -type f -print0 | while IFS= read -r -d '' file; do
extension="${file##*.}"
if [[ $extension != "gitignore" ]]; then
    cat <<BLOCK
    app.get("${file:1}", [&](uWS::HttpResponse<false>* res, uWS::HttpRequest*) {
            const char msg[]{
            $(cat "$file" | xxd -i)
        };
        res->writeHeader("Content-type", "$(get_content_type "$file")");
        res->end(std::string_view(msg,sizeof(msg)), true);
    });
BLOCK
fi
done
