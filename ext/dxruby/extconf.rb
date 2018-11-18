require "mkmf"

SYSTEM_LIBRARIES = [
  "dxguid",
  "d3d9",
  ["d3dx9_40", "d3dx9"],
  "dinput8",
  "dsound",
  "gdi32",
  "ole32",
  "user32",
  "kernel32",
  "comdlg32",
  "winmm",
  "uuid",
  "imm32",
]

SYSTEM_LIBRARIES.each do |libs|
  [*libs].any? {|lib| have_library(lib) }
end

#ヘッダファイル足りてませんが調べるの面倒で(^-^;

have_header("d3dx9.h")
have_header("dinput.h")
have_func("rb_enc_str_new") 

create_makefile("dxruby")
