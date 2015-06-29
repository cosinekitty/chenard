/* ui.ts - Don Cross - http://cosinekitty.com */

/// <reference path="jquery.d.ts" />
/// <reference path="fen.ts" />

function MakeImageContainer(x:number, y:number): string {
    return '<div id="Square_' + x.toString() + y.toString() + '"' +
        ' style="position:absolute; left:' + (44*x).toFixed() + 'px; top:' + (44*(7-y)).toFixed() + 'px;"></div>';
}

function MakeImageHtml(filename:string): string {
    return '<img src="' + filename + '" width="44" height="44"/>';
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
    $('#DivBoard').html(html);
    SetBoard('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR');
}

function SetBoard(text:string):boolean {
    if (UpdateBoard(text)) {
        $('#TextBoxFen').val(text);
        return true;
    }
    return false;
}

var RotateFlag:boolean = false;

function UpdateBoard(text:string):boolean {
    var board: fen.ChessBoard;
    try {
        board = new fen.ChessBoard(text);
    } catch (ex) {
        $('#DivErrorText').text(ex);
        ClearBoard();
        return false;
    }

    var x, rx, y, ry:number;
    var imageFileName: string;
    for (y=0; y < 8; ++y) {
        ry = RotateFlag ? (7-y) : y;
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
        $('#DivErrorText').text('');
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

    $('#ButtonClear').click(function(){
        SetBoard('8/8/8/8/8/8/8/8');
    });

    InitBoardDisplay();
});
