/* ui.ts - Don Cross - http://cosinekitty.com */

/// <reference path="jquery.d.ts" />
/// <reference path="fen.ts" />

function MakeImageHtml(x:number, y:number, filename:string): string {
    return '<td id="Square_' + x.toString() + y.toString() + '"><img src="' + filename + '" width="44" height="44" style="display:block;"/></td>';
}

function InitBoardDisplay() {
    var x:number;
    var y:number;
    var imageFileNames:string[] = ['bitmap/bsq.png', 'bitmap/wsq.png'];
    var html:string = '<table cellspacing="1" cellpadding="1" border="1">\n';
    for (y=7; y>=0; --y) {
        html += '<tr>';
        for (x=0; x<8; ++x) {
            html += MakeImageHtml(x, y, imageFileNames[(x+y)&1]);
        }
        html += '</tr>';
    }
    html += '</table>\n';
    $('#DivBoard').html(html);
}

$(function(){
    $('#ButtonDisplay').click(function(){
        $('#DivErrorText').text('');
        var text: string = $('#TextBoxFen').prop('value');
        var board: fen.ChessBoard;
        try {
            board = new fen.ChessBoard(text);
        } catch (ex) {
            $('#DivErrorText').text(ex);
            return;
        }

        var x:number;
        var y:number;
        var imageFileName: string;
        for (y=0; y < 8; ++y) {
            for (x=0; x < 8; ++x) {
                imageFileName = 'bitmap/' + board.ImageFileName(x, y);
                $('#Square_' + x.toString() + y.toString()).html(MakeImageHtml(x, y, imageFileName));
            }
        }
    });

    InitBoardDisplay();
});
