// --------------------------------------------------------------------
// Ajax support
// --------------------------------------------------------------------

function updateControl( param, val )
{
  var xmlhttp = new XMLHttpRequest();
  console.log( "updateControl", param, val );

  xmlhttp.onreadystatechange = function()
  {
    if ( this.readyState == 4 && this.status == 200 )
    {
      setControl( this );
    }
  };

  var request = "Config.cgi?"+param+"="+val;
  console.log( "Request: ", request );
  xmlhttp.open( "GET", request, true );
  xmlhttp.send();
}

function setControl( xhttp )
{
   console.log( xhttp );
   var resp = JSON.parse( xhttp.response );
   console.log( "JSON Response. param: ", resp.param, "value: ",  resp.value );
   console.log( "Response: ", xhttp.responseText );
   document.getElementById( resp.param ).value = resp.value;
}

// --------------------------------------------------------------------
// cookie support
// --------------------------------------------------------------------

function setCookie( cname, cvalue, exdays )
{
   console.log( "setCookie", cname, cvalue );
   var d = new Date();
   d.setTime( d.getTime() + ( exdays * 24 * 60 * 60 * 1000 ) );
   var expires = "expires=" + d.toUTCString();
   document.cookie = cname + "=" + cvalue + ";" + expires + ";path=/";
}

function getCookie( cname )
{
   var name = cname + "=";
   var ca = document.cookie.split( ';' );
   for( var i = 0; i < ca.length; i++ )
   {
      var c = ca[i];
      while( c.charAt( 0 ) == ' ' )
      {
         c = c.substring( 1 );
      }
      if( c.indexOf( name ) == 0 )
      {
          var cvalue = c.substring( name.length, c.length );
          console.log( "getCookie", cname, cvalue );
          return cvalue;
      }
   }
   return "";
}

// --------------------------------------------------------------------
// item helper
// --------------------------------------------------------------------

function itemShowHide( _img, _item, op )
{
   console.log( "itemShowHide", _img, _item, op );
   // search for img
   var img  = document.getElementById( _img );
   var item = document.getElementById( _item );
   if( op === "toggle" )
   {
      if( item.style.display == "none" )
      {
         item.style.display = "block";
         img.src = "img/arrow_up.png";
      }
      else
      {
         item.style.display = "none";
         img.src = "img/arrow_down.png";
      }
   }
   else if( op === "show" )
   {
      item.style.display = "block";
      img.src = "img/arrow_up.png";
   }
   else if( op === "hide" )
   {
      item.style.display = "none";
      img.src = "img/arrow_down.png";
   }

   setCookie( _item, item.style.display === "none" ? "hide" : "show", 365 );
}

// --------------------------------------------------------------------
// checkbox support
// --------------------------------------------------------------------

// https://www.annedorko.com/toggle-form-field
function checkboxToggle( checkboxID )
{
   console.log( "checkboxToggle", checkboxID );

   var checkbox = document.getElementById( checkboxID );
   var toggle   = document.getElementById( "hidden_" + checkboxID );
   updateToggle = checkbox.checked ? toggle.disabled = true : toggle.disabled = false;

   updateControl( checkboxID, updateToggle ? 1 : 0 );
}

// --------------------------------------------------------------------
// navbar function
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// Toggle between adding and removing the "responsive" class to topnav
//  when the user clicks on the icon

function navbar()
{
   var x = document.getElementById( "myTopnav" );
   if( x.className === "navbar" )
   {
      x.className += " responsive";
   }
   else
   {
      x.className = "navbar";
   }
}
