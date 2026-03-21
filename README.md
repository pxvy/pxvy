# pxvy

The modern video player for Windows.

https://github.com/shinchiro/mpv-winbuild-cmake/releases

## Color

* Dark mode:
  * Background: `#222529` (34, 37, 41)
  * Foreground: ``#FFFFFF` (255,255,255)



## Build

* [Python 3.13](https://www.python.org/downloads/release/python-31312/)
  * Meson: `pip install meson`
* [MSYS2](https://www.msys2.org/)
  * `C:\msys64`
* [MinGW64](https://github.com/niXman/mingw-builds-binaries/tags)
  * `C:\mingw64`
* [Strawberry Perl](https://strawberryperl.com/)
  * `C:\Strawberry`


```bash
x86_64-15.2.0-release-mcf-seh-ucrt-rt_v13-rev1.7z
x86_64-15.2.0-release-posix-seh-msvcrt-rt_v13-rev1.7z
x86_64-15.2.0-release-posix-seh-ucrt-rt_v13-rev1.7z
x86_64-15.2.0-release-win32-seh-msvcrt-rt_v13-rev1.7z
x86_64-15.2.0-release-win32-seh-ucrt-rt_v13-rev1.7z

```

```
C:\msys64\mingw64\bin
```

```bash
avcodec-62.dll
avdevice-62.dll
avfilter-11.dll
avformat-62.dll
avutil-60.dll
libass-9.dll
libbrotlicommon.dll
libbrotlidec.dll
libbz2-1.dll
libcrypto-3-x64.dll
libdovi.dll
libfontconfig-1.dll
libfreetype-6.dll
libfribidi-0.dll
libglib-2.0-0.dll
libgraphite2.dll
libharfbuzz-0.dll
libjpeg-8.dll
liblcms2-2.dll
libmpv-2.dll
libpcre2-8-0.dll
libplacebo-351.dll
libpng16-16.dll
libshaderc_shared.dll
libspirv-cross-c-shared.dll
libunibreak-6.dll
swresample-6.dll
swscale-9.dl
```

## 3rdparty

* [SQLite](https://www.sqlite.org/download.html)
* [libmpv](https://github.com/shinchiro/mpv-winbuild-cmake/releases)
* 

## 탐색기

```
powershell [guid]::NewGuid()
```



# TODO	

- [x] Mediainfo TAB키로 ON/OFF
- [ ] PlayList가 IINA에 있나?
  - [ ] 있다면 PlayList만들기
- [ ] 탐색기 썸네일
- [ ] 설정 창
  - [ ] 자막 폰트 변경
  - [ ] 

- [ ] 잘못된 파일이 열리면 경고메시지 표시
- [ ] PIP 모드
- [ ] About 창
  - [ ] 라이선스 표시 (GPL 사용하자 공짜로 배포하고)
- [ ] 라이트 모드 대응하기
- [x] 최근 재생 목록
  - [x] 10개로 제한
  - [x] (Clear) 추가




## Tools (python 3.13)



* smi to srt



## 단축키

* TAB: 재생정보 출력
* F: fps 출력
* X: 재생속도 -0.1
* C: 재생속도 +0.1
* ENTER: 전체화면 전환
* ESC: 전체화면 해제, 창 최소화
* Ctrl + WHEEL: 시스템 볼륨 조정
* Ctrl + Q: 프로그램 종료
* Ctrl + O: 파일 열기
* H: 캡션바, 컨트롤뷰, 커서 숨기기
* 

 
