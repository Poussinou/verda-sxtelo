/*
 * Verda Ŝtelo - An anagram game in Esperanto for the web
 * Copyright (C) 2011, 2012, 2013  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

var KEEP_ALIVE_TIME = 2.5 * 60 * 1000;

function getAjaxObject ()
{
  /* On IE we'll use XDomainRequest but make it look like an
   * XMLHttpRequest object */
  if (window["XDomainRequest"])
    {
      var obj = {};
      obj.xdr = new XDomainRequest ();

      obj.xdr.onprogress = function ()
        {
          obj.readyState = 3;
          obj.responseText = obj.xdr.responseText;
          if (obj.onreadystatechange)
            obj.onreadystatechange ();
        };
      obj.xdr.onload = function ()
        {
          obj.status = 200;
          obj.readyState = 4;
          obj.responseText = obj.xdr.responseText;
          if (obj.onreadystatechange)
            obj.onreadystatechange ();
        };
      obj.xdr.onerror = function ()
        {
          obj.status = 300;
          obj.readyState = 4;
          if (obj.onreadystatechange)
            obj.onreadystatechange ();
        }
      obj.xdr.ontimeout = obj.xdr.onerror;

      obj.setRequestHeader = function () { };

      obj.abort = function () { this.xdr.abort (); };
      obj.open = function () { this.xdr.open.apply (this.xdr, arguments); };
      obj.send = function () { this.xdr.send.apply (this.xdr, arguments); };

      return obj;
    }
  else
    return new XMLHttpRequest ();
}

function ChatSession ()
{
  this.terminatorRegexp = /\r\n/;
  this.personId = null;
  this.personNumber = null;
  this.messageNumber = 0;
  this.messageQueue = [];
  this.sentTypingState = false;
  this.unreadMessages = 0;
  this.players = [];
  this.tiles = [];

  this.playerName = "ludanto";

  var search = window.location.search;
  if (search && search.match (/^\?[a-z]+$/))
    this.roomName = search.substring (1);
  else
    this.roomName = "default";
}

ChatSession.prototype.setState = function (state)
{
  this.state = state;

  if (state == "in-progress")
  {
    $("#message-input-box").removeAttr ("disabled");
    $("#submit-message").removeAttr ("disabled");
  }
  else
  {
    $("#message-input-box").attr ("disabled", "disabled");
    $("#submit-message").attr ("disabled", "disabled");
  }
};

ChatSession.prototype.getUrl = function (method)
{
  var location = window.location;
  return "http://" + location.hostname + ":5142/" + method;
};

ChatSession.prototype.clearWatchAjax = function ()
{
  var watchAjax = this.watchAjax;
  /* Clear the Ajax object first incase aborting it fires a
   * readystatechange event */
  this.watchAjax = null;
  if (watchAjax)
    watchAjax.abort ();
};

ChatSession.prototype.setError = function (msg)
{
  if (!msg)
    msg = "@ERROR_OCCURRED@";

  this.clearWatchAjax ();
  this.clearCheckDataInterval ();

  $("#status-note").text (msg);
  this.setState ("error");
};

ChatSession.prototype.handleHeader = function (header)
{
  if (typeof (header) != "object")
    this.setError ("@BAD_DATA@");

  /* Ignore the header if we've already got further than this state */
  if (this.state != "connecting")
    return;

  this.personNumber = header.num;
  this.personId = header.id;

  $("#status-note").text ("");
  this.setState ("in-progress");
};

ChatSession.prototype.handlePlayerName = function (data)
{
  if (typeof (data) != "object")
    this.setError ("@BAD_DATA@");

  if (this.state != "in-progress")
    return;

  var player = this.getPlayer (data.num);
  player.name = data.name;
  player.element.textContent = player.name;
};

ChatSession.prototype.handlePlayer = function (data)
{
  if (typeof (data) != "object")
    this.setError ("@BAD_DATA@");

  if (this.state != "in-progress")
    return;

  var player = this.getPlayer (data.num);
  player.typing = data.typing;
  player.connected = data.connected;

  var className = "player";
  if (data.typing)
    className += " typing"
  if (!data.connected)
    className += " disconnected";

  player.element.className = className;
};

ChatSession.prototype.handleEnd = function ()
{
  if (this.state == "in-progress")
  {
    $("#status-note").text ("@PERSON_LEFT@");
    this.setState ("done");
    $("#conversation-finished-note").show ();
  }
};

ChatSession.prototype.handleChatMessage = function (message)
{
  if (typeof (message) != "object")
    this.setError ("@BAD_DATA@");

  /* Ignore the message if we've already got further than this state */
  if (this.state != "in-progress")
    return;

  var div = $(document.createElement ("div"));
  div.addClass ("message");

  var span = $(document.createElement ("span"));
  div.append (span);
  span.text (this.getPlayer (message.person).name);
  span.addClass (message.person == this.personNumber
                 ? "message-you" : "message-stranger");

  div.append ($(document.createTextNode (" " + message.text)));

  $("#messages").append (div);

  var messagesBox = $("#messages-box");
  messagesBox.scrollTop (messagesBox.prop("scrollHeight"));

  if (document.hasFocus && !document.hasFocus ())
  {
    this.unreadMessages++;
    document.title = "(" + this.unreadMessages + ") Verda Ŝtelo";
    var messageAlertSound = document.getElementById ("message-alert-sound");
    if (messageAlertSound && messageAlertSound.play)
      messageAlertSound.play ();
  }

  this.messageNumber++;
};

ChatSession.prototype.handleTileMessage = function (data)
{
  if (typeof (data) != "object")
    this.setError ("@BAD_DATA@");

  if (this.state != "in-progress")
    return;

  var tile = this.tiles[data.num];

  var new_x = (data.x / 10.0) + "em";
  var new_y = (data.y / 10.0) + "em";

  if (tile == null)
  {
    tile = this.tiles[data.num] = {};
    tile.element = document.createElement ("div");
    tile.element.className = "tile";
    tile.element.style.left = new_x;
    tile.element.style.top = new_y;
    tile.facingUp = false;
    $("#board").append (tile.element);
  }
  else
  {
    if (data.x != tile.x || data.y != tile.y)
    {
      var te = $(tile.element);
      var dx = tile.x - data.x;
      var dy = tile.y - data.y;
      var distance = Math.sqrt (dx * dx + dy * dy);

      te.stop ();
      te.animate ({ "left": new_x, "top": new_y },
                  distance * 2.0);
    }
  }

  tile.x = data.x;
  tile.y = data.y;

  if (data["facing-up"] && !tile.facingUp)
  {
    tile.element.textContent = data.letter;
    tile.facingUp = true;
  }
};

ChatSession.prototype.processMessage = function (message)
{
  if (typeof (message) != "object"
      || typeof (message[0]) != "string")
    this.setError ("@BAD_DATA@");

  switch (message[0])
  {
  case "header":
    this.handleHeader (message[1]);
    break;

  case "end":
    this.handleEnd ();
    break;

  case "player-name":
    this.handlePlayerName (message[1]);
    break;

  case "player":
    this.handlePlayer (message[1]);
    break;

  case "message":
    this.handleChatMessage (message[1]);
    break;

  case "tile":
    this.handleTileMessage (message[1]);
    break;
  }
};

ChatSession.prototype.checkData = function ()
{
  var responseText = this.watchAjax.responseText;

  if (responseText)
    while (this.watchPosition < responseText.length)
    {
      var rest = responseText.slice (this.watchPosition);
      var terminatorPos = rest.search (this.terminatorRegexp);

      if (terminatorPos == -1)
        break;

      try
      {
        var message = eval ('(' + rest.slice (0, terminatorPos) + ')');
      }
      catch (e)
      {
        this.setError ("@BAD_DATA@");
        return;
      }

      this.processMessage (message);

      this.watchPosition += terminatorPos + 2;
    }
};

ChatSession.prototype.clearCheckDataInterval = function ()
{
  if (this.checkDataInterval)
  {
    clearInterval (this.checkDataInterval);
    this.checkDataInterval = null;
  }
};

ChatSession.prototype.resetCheckDataInterval = function ()
{
  this.clearCheckDataInterval ();
  this.checkDataInterval = setInterval (this.checkData.bind (this), 3000);
};

ChatSession.prototype.watchReadyStateChangeCb = function ()
{
  if (!this.watchAjax)
    return;

  /* Every time the ready state changes we'll check for new data in
   * the response and reset the timer. That way browsers that call
   * onreadystatechange whenever new data arrives can get data as soon
   * as it comes it, but other browsers will still eventually get the
   * data from the timeout */

  if (this.watchAjax.readyState == 3)
  {
    this.checkData ();
    this.resetCheckDataInterval ();
  }
  else if (this.watchAjax.readyState == 4)
  {
    if (this.watchAjax.status == 200)
    {
      this.checkData ();
      this.watchAjax = null;
      this.clearCheckDataInterval ();

      /* If we didn't get a complete conversation then restart the
       * query */
      if (this.state != "done")
        this.startWatchAjax ();
    }
    else
    {
      this.watchAjax = null;
      this.setError ();
    }
  }
};

ChatSession.prototype.startWatchAjax = function ()
{
  var method;

  if (this.personId)
    method = "watch_person?" + this.personId + "&" + this.messageNumber;

  else
    method = ("new_person?" + encodeURIComponent (this.roomName) + "&" +
              encodeURIComponent (this.playerName));

  this.clearWatchAjax ();

  this.watchPosition = 0;

  this.watchAjax = getAjaxObject ();

  this.watchAjax.onreadystatechange = this.watchReadyStateChangeCb.bind (this);

  this.watchAjax.open ("GET", this.getUrl (method));
  this.watchAjax.send (null);

  this.resetCheckDataInterval ();
  this.resetKeepAlive ();
};

ChatSession.prototype.watchCompleteCb = function (xhr, status)
{
  if (!this.watchAjax)
    return;

  if (status == "success")
  {
    this.checkData ();
    this.clearCheckDataInterval ();
    this.clearWatchAjax ();

    /* If we didn't get a complete conversation then restart the
     * query */
    if (this.state != "done")
      this.startWatchAjax ();
  }
  else
  {
    this.setError ();
  }
};

ChatSession.prototype.sendMessageReadyStateChangeCb = function ()
{
  if (!this.sendMessageAjax)
    return;

  if (this.sendMessageAjax.readyState == 4)
  {
    if (this.sendMessageAjax.status == 200)
    {
      this.sendMessageAjax = null;
      this.sendNextMessage ();
    }
    else
    {
      this.sendMessageAjax = null;
      this.setError ();
    }
  }
};

ChatSession.prototype.sendNextMessage = function ()
{
  if (this.sendMessageAjax)
    return;

  if (this.messageQueue.length < 1)
  {
    /* Check if we need to update the typing state */
    var newTypingState = $("#message-input-box").val ().length > 0;
    if (newTypingState != this.sentTypingState)
    {
      this.sentTypingState = newTypingState;

      this.sendMessageAjax = getAjaxObject ();
      this.sendMessageAjax.onreadystatechange =
        this.sendMessageReadyStateChangeCb.bind (this);
      this.sendMessageAjax.open ("GET",
                                 this.getUrl ((newTypingState ?
                                               "start_typing?" :
                                               "stop_typing?") +
                                              this.personId));
      this.sendMessageAjax.setRequestHeader ("Content-Type",
                                             "text/plain; charset=UTF-8");
      this.sendMessageAjax.send (message);

      this.resetKeepAlive ();
    }
    else if ((this.state == "in-progress" ||
              this.state == "awaiting-partner") &&
             ((new Date ()).getTime () -
              this.keepAliveTime.getTime () >=
              KEEP_ALIVE_TIME))
    {
      this.sendMessageAjax = getAjaxObject ();
      this.sendMessageAjax.onreadystatechange =
        this.sendMessageReadyStateChangeCb.bind (this);
      this.sendMessageAjax.open ("GET",
                                 this.getUrl ("keep_alive?" + this.personId));
      this.sendMessageAjax.setRequestHeader ("Content-Type",
                                             "text/plain; charset=UTF-8");
      this.sendMessageAjax.send (message);

      this.resetKeepAlive ();
    }

    return;
  }

  var message = this.messageQueue.shift ();

  this.sendMessageAjax = getAjaxObject ();
  this.sendMessageAjax.onreadystatechange =
    this.sendMessageReadyStateChangeCb.bind (this);

  if (message[0] == "message")
  {
    /* The server assumes we've stopped typing whenever a message is
     * sent */
    this.sentTypingState = false;

    this.sendMessageAjax.open ("POST",
                               this.getUrl ("send_message?" + this.personId));
    this.sendMessageAjax.setRequestHeader ("Content-Type",
                                           "text/plain; charset=UTF-8");
    this.sendMessageAjax.send (message[1]);
  }
  else if (message[0] == "flip-tile")
  {
    this.sendMessageAjax.open ("GET",
                               this.getUrl ("flip_tile?" + this.personId + "&" +
                                            message[1]));
    this.sendMessageAjax.send ();
  }

  this.resetKeepAlive ();
};

ChatSession.prototype.queueCurrentMessage = function ()
{
  var message;

  if (this.state != "in-progress")
    return;

  message = $("#message-input-box").val ();

  if (message.length > 0)
  {
    $("#message-input-box").val ("");
    this.messageQueue.push (["message", message]);
    this.sendNextMessage ();
  }
};

ChatSession.prototype.submitMessageClickCb = function ()
{
  this.queueCurrentMessage ();
};

ChatSession.prototype.keyDownCb = function (event)
{
  if (event.which == 10 || event.which == 13)
  {
    event.preventDefault ();
    this.queueCurrentMessage ();
  }
};

ChatSession.prototype.inputCb = function (event)
{
  /* Maybe update the typing status */
  this.sendNextMessage ();
};

ChatSession.prototype.newConversationCb = function ()
{
  window.location.reload ();
};

ChatSession.prototype.focusCb = function ()
{
  this.unreadMessages = 0;
  document.title = "Verda Ŝtelo";
};

ChatSession.prototype.boardClickCb = function (event)
{
  if (event.button != 0)
    return;

  event.preventDefault ();

  if ((event.target.className) == "tile")
  {
    var i;
    var tileNum;
    var tile = null;

    for (tileNum = 0; tileNum < this.tiles.length; tileNum++)
    {
      tile = this.tiles[tileNum];
      if (tile && tile.element == event.target)
        break;
    }

    if (tile == null || tile.facingUp)
      return;

    /* Make sure that we haven't already queued this flip */
    for (i = 0; i < this.messageQueue.length; i++)
      if (this.messageQueue[i][0] == "flip-tile" &&
          this.messageQueue[i][1] == tileNum)
        return;

    this.messageQueue.push (["flip-tile", tileNum]);
    this.sendNextMessage ();
  }
};

ChatSession.prototype.loadCb = function ()
{
  this.setState ("connecting");
  this.startWatchAjax ();

  $("#submit-message").bind ("click", this.submitMessageClickCb.bind (this));
  $("#message-input-box").bind ("keydown", this.keyDownCb.bind (this));
  $("#message-input-box").bind ("input", this.inputCb.bind (this));
  $("#new-conversation-button").bind ("click",
                                      this.newConversationCb.bind (this));
  $("#board").bind ("click", this.boardClickCb.bind (this));

  /* Prevent default handling of mouse events on the board because
   * otherwise you can accidentally select the text in a tile */
  var preventDefaultCb = function (event) { event.preventDefault (); };
  $("#board").mousedown (preventDefaultCb);
  $("#board").mouseup (preventDefaultCb);

  $(window).focus (this.focusCb.bind (this));

  $(window).unload (this.unloadCb.bind (this));
};

ChatSession.prototype.unloadCb = function ()
{
  if (this.personId)
  {
    /* Try to squeeze in a synchronous Ajax to let the server know the
     * person has left */
    var ajax = getAjaxObject ();

    ajax.open ("GET", this.getUrl ("leave?" + this.personId),
               false /* not asynchronous */);
    ajax.send (null);

    /* If this is an XDomainRequest then making it asynchronous
     * doesn't work but we can do a synchronous request back to the
     * same domain. With any luck this will cause it to wait long
     * enough to also flush the leave request */
    if (ajax.xdr)
      {
        var xhr = new XMLHttpRequest ();
        xhr.open ("GET", window.location.url, false);
        xhr.send (null);
      }
  }
};

ChatSession.prototype.resetKeepAlive = function ()
{
  if (this.keepAliveTimeout)
    clearTimeout (this.keepAliveTimeout);
  this.keepAliveTime = new Date ();
  this.keepAliveTimeout = setTimeout (this.sendNextMessage.bind (this),
                                      KEEP_ALIVE_TIME);
};

ChatSession.prototype.getPlayer = function (playerNum)
{
  var player;

  if (!(player = this.players[playerNum]))
  {
    player = this.players[playerNum] = {};

    player.typing = false;
    player.connected = true;
    player.name = "";

    player.element = document.getElementById ("player-" + playerNum);
    if (player.element == null)
    {
      player.element = document.createElement ("span");
      $("#other-players").append (player.element);
      $("#other-players").show ();
    }
  }

  return player;
};

/* .bind is only implemented in recent browsers so this provides a
 * fallback if it's not available. Verda Ŝtelo only ever uses it bind the
 * 'this' context so it doesn't bother with any other arguments */
if (!Function.prototype.bind)
  {
    Function.prototype.bind = function (obj)
    {
      var originalFunc = this;
      return function () {
        return originalFunc.apply (obj, [].slice.call (arguments));
      };
    };
  }

(function ()
{
  var cs = new ChatSession ();
  $(window).load (cs.loadCb.bind (cs));
}) ();
