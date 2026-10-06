import numpy as np, sys
from scipy.ndimage import label
MAP,TD,W,H=sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4])
BW,BH=W//8,H//8
fl=np.zeros(0x4000,np.uint64)
with open(TD,'rb') as f:
    for g in range(512):
        b=f.read(964)
        for e in range(32): fl[g*32+e]=int.from_bytes(b[4+e*30:4+e*30+8],'little')
wet=(fl&np.uint64(0x80))!=0; imp=(fl&np.uint64(0x40))!=0
raw=np.fromfile(MAP,np.uint8).reshape(BW*BH,196); c=raw[:,4:].reshape(BW*BH,64,3)
ids=(c[:,:,0].astype(np.uint16)|(c[:,:,1].astype(np.uint16)<<8))
ID=np.zeros((H,W),np.uint16); bi=np.arange(BW*BH);bx=bi//BH;by=bi%BH;cc=np.arange(64);cx=cc%8;cyy=cc//8
for k in range(64): ID[by*8+cyy[k],bx*8+cx[k]]=ids[:,k]
walk=(~wet[ID])&(~imp[ID])
lab,nc=label(walk); sizes=np.bincount(lab.ravel()); sizes[0]=0
big=(sizes>=20000).sum()
print(f"components={nc} | >=20k tiles={big} | largest={sizes.max():,} | total walkable={int(walk.sum()):,}")
