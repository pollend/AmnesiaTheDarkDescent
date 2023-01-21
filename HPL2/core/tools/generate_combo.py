import argparse
from itertools import compress, product
from os import path

parser = argparse.ArgumentParser(description='generate combos.')
parser.add_argument('-n', '--name')
parser.add_argument('-i', '--input')
parser.add_argument('-o', '--output')
parser.add_argument('-f', '--features', nargs='*')

args = parser.parse_args()

def combinations(items):
    return ( set(compress(items,mask)) for mask in product(*[[0,1]]*len(items)) )

bits = {}
for idx, feat in enumerate(args.features):
    bits[feat] = (1 << idx)

input_text = ""
with open(args.input, "r") as input_file:
    input_text = input_file.read()

for combo in combinations(args.features):
    supported = 0
    for c in combo:
        supported |= bits[c]
    with open(path.join(args.output, "%s_c%i.sc" % (args.name, supported)), "w") as file:
        
        for c in combo:
            file.write("#define %s 1 \n" % (c))
        file.write("\n")
        file.write(input_text)
        

