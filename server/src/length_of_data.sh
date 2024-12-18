serial_file="$1"
if [ ! -f $serial_file ]; then
	echo "not a serial output file: $1"
	exit
fi

grep '>=< \[[0-9a-f]\+\.00\] ' $serial_file \
	| python3 -c '
cumulated_length = 0
while True:
	try:
		line = input()
	except EOFError:
		break
	_, marker, offsets, block_id, length, flags, reserved, _ = line.split(" ")
	block_id = int(block_id, 16)
	length = int(length, 16)
	if flags == "0000000000000001":
		print(f"{block_id = :3x}, {length = :3x} redundancy.")
		continue
	
	cumulated_length += length
	print(f"{block_id = :3x}, {length = :3x} -> total {cumulated_length:8x} words, {cumulated_length*8:8x} bytes")
'
