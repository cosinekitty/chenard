/* fen.ts - Don Cross - http://cosinekitty.com */

module fen {
    export enum ChessSquare {
        EMPTY,

        WPAWN,
        WKNIGHT,
        WBISHOP,
        WROOK,
        WQUEEN,
        WKING,

        BPAWN,
        BKNIGHT,
        BBISHOP,
        BROOK,
        BQUEEN,
        BKING,
    }

    export class ChessBoard {
        private squares: ChessSquare[];

        constructor(fentext: string) {
            // r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3

            this.squares = [];

            // Split FEN text into space-delimited tokens.
            var stokens:string[] = fentext.split(' ');

            // Let's be pretty tolerant. We care only about the board layout, so search
            // for it.  Assume it is the first token that contains a forward slash.
            var i:number;
            var layout:string = null;
            for (i=0; i < stokens.length; ++i) {
                if (stokens[i].indexOf('/') > 0) {      // note we exclude beginning with a slash on purpose
                    layout = stokens[i];
                    break;
                }
            }
            if (layout === null) {
                throw 'Cannot find board layout in FEN text.';
            }

            // Prevent abuse by checking for excessive layout length.
            // The longest possible layout would have 8 ranks with 8 characters each,
            // with 7 slashes separating them.
            if (layout.length > 8*8 + 7) {
                throw 'Invalid FEN layout: too many characters.';
            }

            // Split the FEN text into 8 ranks.
            var ranks:string[] = layout.split('/');
            if (ranks.length != 8) {
                throw 'Invalid FEN layout: must have exactly 8 ranks.';
            }

            // Process the ranks backwards, so that we handle White's side of the board first.
            var r:number;
            var numEmpty:number;
            var c:string;
            for (r=7; r >= 0; --r) {
                // Decode each rank string into 8 squares.
                for (i=0; i < ranks[r].length; ++i) {
                    switch (c = ranks[r].charAt(i)) {
                        case 'P': this.squares.push(ChessSquare.WPAWN);      break;
                        case 'N': this.squares.push(ChessSquare.WKNIGHT);    break;
                        case 'B': this.squares.push(ChessSquare.WBISHOP);    break;
                        case 'R': this.squares.push(ChessSquare.WROOK);      break;
                        case 'Q': this.squares.push(ChessSquare.WQUEEN);     break;
                        case 'K': this.squares.push(ChessSquare.WKING);      break;
                        case 'p': this.squares.push(ChessSquare.BPAWN);      break;
                        case 'n': this.squares.push(ChessSquare.BKNIGHT);    break;
                        case 'b': this.squares.push(ChessSquare.BBISHOP);    break;
                        case 'r': this.squares.push(ChessSquare.BROOK);      break;
                        case 'q': this.squares.push(ChessSquare.BQUEEN);     break;
                        case 'k': this.squares.push(ChessSquare.BKING);      break;
                        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
                            for (numEmpty = +c; numEmpty > 0; --numEmpty) {
                                this.squares.push(ChessSquare.EMPTY);
                            }
                            break;
                        default:
                            throw 'Invalid character in FEN layout text.';
                    }
                }
            }

            if (this.squares.length != 64) {
                throw 'Invalid number of squares encoded by FEN layout text: must be 64.';
            }
        }

        public Square(x:number, y:number): ChessSquare {
            if (!ChessBoard.IsInteger(x) || (x<0) || (x>7)) {
                throw "x coordinate must be integer in the range 0..7";
            }
            if (!ChessBoard.IsInteger(y) || (y<0) || (y>7)) {
                throw "y coordinate must be integer in the range 0..7";
            }
            return this.squares[(8*y) + x];
        }

        private static IsInteger(n:number): boolean {
            return !isNaN(n) && ((n | 0) === n);
        }

        public ImageFileName(x:number, y:number): string {
            var sq = this.Square(x, y);     // will validate coordinates for us
            var bg:string = 'bw'.charAt((x+y) & 1);     // square background color code: 'b' or 'w'
            switch (sq) {
                case ChessSquare.WPAWN:     return bg + 'wp' + '.png';
                case ChessSquare.WKNIGHT:   return bg + 'wn' + '.png';
                case ChessSquare.WBISHOP:   return bg + 'wb' + '.png';
                case ChessSquare.WROOK:     return bg + 'wr' + '.png';
                case ChessSquare.WQUEEN:    return bg + 'wq' + '.png';
                case ChessSquare.WKING:     return bg + 'wk' + '.png';
                case ChessSquare.BPAWN:     return bg + 'bp' + '.png';
                case ChessSquare.BKNIGHT:   return bg + 'bn' + '.png';
                case ChessSquare.BBISHOP:   return bg + 'bb' + '.png';
                case ChessSquare.BROOK:     return bg + 'br' + '.png';
                case ChessSquare.BQUEEN:    return bg + 'bq' + '.png';
                case ChessSquare.BKING:     return bg + 'bk' + '.png';
                default:                    return bg + 'sq.png';
            }
        }
    }
}
