# $Id: changelog.sed,v 1.2 2001/06/23 00:07:03 oskuro Exp $
# Changes cvs2cl's output to something more pleasant
s/\([0-9][0-9]:[0-9][0-9]\) /\1 UTC --/g

# Add yourself below:
s/dwonis/Dwayne C. Litzenberger <dlitz@dlitz.net>/g
s/oskuro/Jordi Mallach <jordi@sindominio.net>/g
s/markph/Mark Hurley <debian4tux@telocity.com>/g
