FROM alpine:3.17.2 AS build
RUN apk add meson libgcc musl-dev gcc g++ upx
COPY . /code
RUN mkdir /build
WORKDIR /code
RUN --mount=type=cache,target=/build LDFLAGS='-static' meson /build/code --default-library static --buildtype=release
WORKDIR /build/code
RUN  --mount=type=cache,target=/build ninja
RUN mkdir /install
RUN --mount=type=cache,target=/build DESTDIR=/install meson install
RUN upx /install/usr/local/bin/wart-node

FROM scratch AS export-stage
COPY --from=build install/usr/local/bin/wart-node .
COPY --from=build install/usr/local/bin/wart-wallet .
