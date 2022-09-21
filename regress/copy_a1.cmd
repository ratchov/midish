load "bend1.msh"
tnew u
ct t; g 1; sel 1; tcopy; ct u; g 0; tpaste; g 0; sel 0
ct t; tdel
g 0; sel 0; ct nil; ci nil; co nil
