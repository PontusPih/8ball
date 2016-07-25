mem[07756] = 06032; // KCC clear AC and keyboard/reader flag
mem[07757] = 06031; // KSF skip if keyboard/reader flag = 1
mem[07760] = 05357; // JMP within this memory page (1) address 157 (absolute 7757, above)
mem[07761] = 06036; // KRB clear AC, read keyboard buffer, clear keyboard flag
mem[07762] = 07106; // CLL RTL clear link, rotate 2 left
mem[07763] = 07006; // RTL rotate AC and link left two
mem[07764] = 07510; // SPA skip on plus AC
mem[07765] = 05357; // JMP to memory page 1 address 157 (absolute 7757)
mem[07766] = 07006; // RTL rotate AC and link left two
mem[07767] = 06031; // KSF skip if keyboard/reader flag = 1
mem[07770] = 05367; // JMP within this memory page (1) address 167 (absolute 7767, above)
mem[07771] = 06034; // KRS read keyboard/reader buffer, static
mem[07772] = 07420; // SNL skip on non-zero link
mem[07773] = 03776; // DCA deposit and clear AC indirect memory page 1, address [7776]
mem[07774] = 03376; // DCA deposit and clear AC memory page1, address 176 (absolute 7776)
mem[07775] = 05356; // JMP within this memory page (1) address 156 (absolute 7756)
