

class Space:
    def __init__(self, start, size):
        self.start = start
        self.size = size



def canMerge(space1, space2):

    left = space1.start
    right = space1.start + space1.size

    sLeft = space2.start
    sRight = space2.start + space2.size

    return left <= sRight and right >= sLeft; 

def Merge(space1, space2):
    merged = Space(0,0)
    merged.start = min(space2.start, space1.start)
    merged.size = max(space1.start + space1.size, space2.start + space2.size) - merged.start
    return merged



space1 = Space(6, 3)
space2 = Space(10, 8)
merged = Merge(space1, space2)

print(canMerge(space1, space2))
print(merged.start, merged.size)