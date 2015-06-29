/* ui.ts - Don Cross - http://cosinekitty.com */

/// <reference path="jquery.d.ts" />
/// <reference path="fen.ts" />

function MakeImageContainer(x:number, y:number, filename:string): string {
    return '<div id="Square_' + x.toString() + y.toString() + '"' +
        ' src="' + filename + '"' +
        ' style="position:absolute; left:' + (44*x).toFixed() + 'px; top:' + (44*y).toFixed() + 'px;">' +
        MakeImageHtml(filename) +
        '</div>';
}

function MakeImageHtml(filename:string): string {
    return '<img src="' + filename + '" width="44" height="44"/>';
}

function InitBoardDisplay() {
    var x:number;
    var y:number;
    var imageFileNames:string[] = ['bitmap/bsq.png', 'bitmap/wsq.png'];
    var html:string = '';
    for (y=7; y>=0; --y) {
        for (x=0; x<8; ++x) {
            html += MakeImageContainer(x, y, imageFileNames[(x+y)&1]);
        }
    }
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
                $('#Square_' + x.toString() + y.toString()).html(MakeImageHtml(imageFileName));
            }
        }
    });

    $('#TextBoxFen').click(function(){
        $(this).select();
    });

    InitBoardDisplay();
});
