# $Id: changelog.sed,v 1.1 2001/06/19 03:38:58 dwonis Exp $
# Changes cvs2cl's output to something more pleasant
s/\([0-9][0-9]:[0-9][0-9]\) /\1 UTC --/g

# Add yourself below:
s/dwonis/Dwayne C. Litzenberger <dlitz@dlitz.net>/g
s/oskuro/Jordi Mallach <jordi@sindominio.net>/g
#s/????/Mark Hurley <debian4tux@telocity.com>/g
