def generate_c_code(file_path, items_per_row=32):
    amount = 6912
    with open(file_path, 'rb') as file:
        data = file.read(amount)

    c_code = ''
    for i, byte in enumerate(data):
        if i % items_per_row == 0:
            c_code += '\n'

        c_code += '0x{:02X}, '.format(byte)

    # Remove the trailing comma and space
    c_code = c_code[:-2]

    c_code = 'const unsigned char rawData[' + str(amount) + '] = {' + c_code + '\n};'

    return c_code


# Usage example
#input_file_path = 'output//Judge-Dredd-Featured.png.scr'
input_file_path = 'output//test2.zx.scr'
c_code_data = generate_c_code(input_file_path, items_per_row=32)
print(c_code_data)
