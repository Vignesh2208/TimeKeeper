#!/bin/sh
# $Id: pdf2eps,v 0.01 2005/10/28 00:55:46 Herbert Voss Exp $
# Convert PDF to encapsulated PostScript.
# usage:
# pdf2eps <page number> <pdf file without ext>

pdfcrop $2.pdf
pdftops -f $1 -l $1 -eps "$2-crop.pdf" 
rm  "$2-crop.pdf"
mv  "$2-crop.eps" $2.eps
