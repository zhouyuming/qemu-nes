# qemu-nes

基于QEMU和[litenes](https://github.com/NJU-ProjectN/LiteNES) 模拟NES游戏机

修复了later分支的多处bug，见https://wiki.huawei.com/domains/4919/wiki/12833/WIKI202308221861603

目前可以正常进行游戏，但会比较卡，且手柄按键功能没有完全实现

## Run

```
mkdir build
cd build
../configure --target-list="nes6502-softmmu" --enable-sdl
make -j16
```
