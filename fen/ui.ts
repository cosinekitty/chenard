/* ui.ts - Don Cross - http://cosinekitty.com */

/// <reference path="jquery.d.ts" />
/// <reference path="fen.ts" />

var RotateFlag:boolean = false;
var SquarePixels:number = 44;

function MakeImageContainer(x:number, y:number): string {
    return '<div id="Square_' + x.toString() + y.toString() + '"' +
        ' style="position:absolute; left:' +
        (SquarePixels*x).toFixed() + 'px; top:' +
        (SquarePixels*(7-y)).toFixed() + 'px;"></div>';
}

function MakeImageHtml(filename:string): string {
    return '<img src="' + filename + '" width="'+SquarePixels+'" height="'+SquarePixels+'"/>';
}

function MakeFileLabel(x:number): string {
    return '<div class="RankFileText" id="FileLabel_' + x.toFixed() + '"' +
        ' style="position: absolute; top: ' + (SquarePixels*8 + 4).toFixed() + 'px; ' +
        ' left: ' + (SquarePixels*x + 19).toFixed() + 'px; ">x</div>';
}

function MakeRankLabel(y:number): string {
    return '<div class="RankFileText" id="RankLabel_' + y.toFixed() + '"' +
        ' style="position: absolute; left:-12px; top:' +
        (SquarePixels*y + 15).toFixed() + 'px;' +
        '">y</div>';
}

function InitBoardDisplay() {
    var x:number;
    var y:number;
    var html:string = '';
    for (y=7; y>=0; --y) {
        for (x=0; x<8; ++x) {
            html += MakeImageContainer(x, y);
        }
    }
    for (x=0; x<8; ++x) {
        html += MakeFileLabel(x);
    }
    for (y=0; y<8; ++y) {
        html += MakeRankLabel(y);
    }
    $('#DivBoard').html(html);
    ResetBoard();
}

function ResetBoard() {
    SetBoard('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR');
}

function SetBoard(text:string):boolean {
    if (UpdateBoard(text)) {
        $('#TextBoxFen').val(text);
        return true;
    }
    return false;
}

function UpdateBoard(text:string):boolean {
    $('#DivErrorText').text('');
    var board: fen.ChessBoard;
    try {
        board = new fen.ChessBoard(text);
    } catch (ex) {
        ClearBoard();   // must clear before setting error text, because this will clear the error text!
        $('#DivErrorText').text(ex);
        return false;
    }

    var x, rx, y, ry:number;
    var imageFileName: string;
    for (y=0; y < 8; ++y) {
        ry = RotateFlag ? (7-y) : y;
        $('#RankLabel_' + ry.toFixed()).text('87654321'.charAt(y));
        $('#FileLabel_' + ry.toFixed()).text('abcdefgh'.charAt(y));
        for (x=0; x < 8; ++x) {
            rx = RotateFlag ? (7-x) : x;
            imageFileName = 'bitmap/' + board.ImageFileName(x, y);
            $('#Square_' + rx.toString() + ry.toString()).html(MakeImageHtml(imageFileName));
        }
    }
    return true;
}

function ClearBoard():void {
    UpdateBoard('8/8/8/8/8/8/8/8');
}

$(function(){
    var textBoxFen = $('#TextBoxFen');

    $('#ButtonDisplay').click(function(){
        UpdateBoard(textBoxFen.prop('value'));
    });

    textBoxFen.click(function(){
        $(this).select();
    }).keyup(function(e){
        if (e.keyCode == 13) {
            UpdateBoard(textBoxFen.prop('value'));
            $(this).select();
        }
    });

    $('#ButtonRotate').click(function(){
        RotateFlag = !RotateFlag;
        UpdateBoard(textBoxFen.prop('value'));
    });

    $('#ButtonReset').click(function(){
        ResetBoard();
    });

    $('#ButtonClear').click(function(){
        SetBoard('8/8/8/8/8/8/8/8');
    });

    InitBoardDisplay();
});
