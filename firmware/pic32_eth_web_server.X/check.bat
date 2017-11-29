set FILE_DEBUG_2=".\dist\pic32mz_ef_sk\production\pic32_eth_web_server.X.production.elf"
set TOOL_2="c:\Program Files (x86)\Microchip\xc32\v1.44\bin"

del *.txt

%TOOL_2%\xc32-readelf -a %FILE_DEBUG_2% > %FILE_DEBUG_2%.sym
c:\windows\system32\sort /+7 /rec 65535 %FILE_DEBUG_2%.sym > %FILE_DEBUG_2%.symbols.txt
copy %FILE_DEBUG_2%.symbols.txt mips.symbols.txt
%TOOL_2%\xc32-objdump -S %FILE_DEBUG_2% > %FILE_DEBUG_2%.disassembly.txt
copy %FILE_DEBUG_2%.disassembly.txt mips.disassembly.txt


