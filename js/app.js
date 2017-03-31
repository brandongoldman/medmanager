<script language="javascript">
function check(form) { /*function to check userid & password*/
/*the following code checkes whether the entered userid and password are matching*/
    if(form.userid.value == "test" && form.password.value == "test") {
        window.open('index.html')/*opens the target page while Id & password matches*/
    }
    
    else {
        alert("Error Password or Username")/*displays error message*/
    }
}
</script>