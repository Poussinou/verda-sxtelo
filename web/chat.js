function ChatSession ()
{
  this.terminatorRegexp = /\r\n/;
  this.personId = null;
  this.personNumber = null;
  this.messagesAdded = 0;
  this.messageQueue = [];
}

ChatSession.prototype.setState = function (state)
{
  this.state = state;

  if (state == "in-progress")
  {
    $("#message-box").removeAttr ("disabled");
    $("#submit-message").removeAttr ("disabled");
  }
  else
  {
    $("#message-box").attr ("disabled", "disabled");
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
    msg = "An error occurred";

  this.clearWatchAjax ();
  this.clearCheckDataInterval ();

  $("#status-note").text (msg);
  this.setState ("error");
};

ChatSession.prototype.handleHeader = function (header)
{
  if (typeof (header) != "object")
    this.setError ("Bad data received");

  /* Ignore the header if we've already got further than this state */
  if (this.state != "connecting")
    return;

  this.personNumber = header.num;
  this.personId = header.id;

  $("#status-note").text ("Waiting for someone to join the conversation");
  this.setState ("awaiting-partner");
};

ChatSession.prototype.handleStateChange = function (newState)
{
  if (typeof (newState) != "string")
    this.setError ("Bad data received");

  switch (newState)
  {
  case "in-progress":
    if (this.state == "awaiting-partner")
    {
      $("#status-note").text ("You are in a conversation. Say hello!");
      this.setState ("in-progress");
    }
    break;

  case "done":
    if (this.state == "in-progress")
    {
      $("#status-note").text ("The other person has left the conversation");
      this.setState ("done");
    }
    break;
  }
};

ChatSession.prototype.handleChatMessage = function (message)
{
  if (typeof (message) != "object")
    this.setError ("Bad data received");

  /* Ignore the message if we've already got further than this state */
  if (this.state != "in-progress")
    return;

  /* Ignore messages that we've already got */
  if (this.messagesAdded <= this.messageNumber)
  {
    var div = $(document.createElement ("div"));
    div.addClass ("message");

    var span = $(document.createElement ("span"));
    div.append (span);
    span.text (message.person == this.personNumber
               ? "You" : "Stranger");
    span.addClass (message.person == this.personNumber
                   ? "message-you" : "message-stranger");

    div.append ($(document.createTextNode (" " + message.text)));

    $("#messages").append (div);

    this.messagesAdded++;
  }

  this.messageNumber++;
};

ChatSession.prototype.processMessage = function (message)
{
  if (typeof (message) != "object"
      || typeof (message[0]) != "string")
    this.setError ("Bad data received");

  switch (message[0])
  {
  case "header":
    this.handleHeader (message[1]);
    break;

  case "state":
    this.handleStateChange (message[1]);
    break;

  case "message":
    this.handleChatMessage (message[1]);
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
        this.setError ("The server sent some invalid data");
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

ChatSession.prototype.watchXhrCb = function ()
{
  var req = $.ajaxSettings.xhr ();
  this.watchAjax = req;
  req.onreadystatechange = this.watchReadyStateChangeCb.bind (this);
  return req;
};

ChatSession.prototype.startWatchAjax = function ()
{
  var method;

  if (this.personId)
    method = "watch_person?" + this.personId;
  else
    method = "new_person?english";

  this.clearWatchAjax ();

  this.watchPosition = 0;
  this.messageNumber = 0;

  this.watchAjax = $.ajaxSettings.xhr ();

  this.watchAjax.onreadystatechange = this.watchReadyStateChangeCb.bind (this);

  this.watchAjax.open ("GET", this.getUrl (method));
  this.watchAjax.send (null);

  this.resetCheckDataInterval ();
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
    return;

  var message = this.messageQueue.shift ();

  this.sendMessageAjax = $.ajaxSettings.xhr ();
  this.sendMessageAjax.onreadystatechange =
    this.sendMessageReadyStateChangeCb.bind (this);
  this.sendMessageAjax.open ("POST",
                             this.getUrl ("send_message?" + this.personId));
  this.sendMessageAjax.setRequestHeader ("Content-Type",
                                         "text/plain; charset=UTF-8");
  this.sendMessageAjax.send (message);
};

ChatSession.prototype.queueCurrentMessage = function ()
{
  var message;

  if (this.state != "in-progress")
    return;

  message = $("#message-box").val ();

  if (message.length > 0)
  {
    $("#message-box").val ("");
    this.messageQueue.push (message);
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

ChatSession.prototype.loadCb = function ()
{
  this.setState ("connecting");
  this.startWatchAjax ();

  $("#submit-message").bind ("click", this.submitMessageClickCb.bind (this));
  $("#message-box").bind ("keydown", this.keyDownCb.bind (this));
};

(function ()
{
  var cs = new ChatSession ();
  $(window).load (cs.loadCb.bind (cs));
}) ();