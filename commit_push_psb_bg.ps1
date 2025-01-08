###################CONFIG##################
$user = "vanishingfork"
$repo = "psb_bg"
$token_filename = "token.env"
###################CONFIG##################
# Get commit message from user
$commitMessage = Read-Host "Enter commit message"
try {
    # Read token from env file
    $envPath = Join-Path $PSScriptRoot $token_filename
    if (!(Test-Path $envPath)) {
        throw "token.env file not found!"
    }
    $token = (Get-Content $envPath).Split('=')[1].Trim()

    # Commit changes
    git add .
    $commitOutput = git commit -am $commitMessage
    Write-Host "Commit successful: $commitOutput" -ForegroundColor Green

    # Push changes using token
    $repoUrl = "https://${user}:${token}@github.com/${user}/${repo}.git"
    $pushOutput = git push $repoUrl
    Write-Host "Push successful: $pushOutput" -ForegroundColor Green
}
catch {
    Write-Host "Error occurred: $_" -ForegroundColor Red
    exit 1
}